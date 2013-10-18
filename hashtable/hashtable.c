#include "hashtable.h"

// 外部から与える関数(下記の物は例)

// キーをハッシュ計算する
// 引数
// key: ハッシュ計算するキー
// 返値
// ハッシュ値
int hashtable_hash_private(void *key)
{
    hashtable_Key *key2 = (hashtable_Key *)key;
    
    // 32 bit の値を上下 16 bit を XOR を取る
    return (key2->addr.s_addr & 0xffff) ^ ((key2->addr.s_addr >> 16) & 0xffff);
}

// キーが同一のものか判定する
// 引数
// key1: キー
// key2: キー
// 返値
// boolean
int hashtable_compareKey_private(void *key1, void *key2)
{
    hashtable_Key *key3, *key4;
    
    key3 = (hashtable_Key *)key1;
    key4 = (hashtable_Key *)key2;
    
    return memcmp(key3, key4, sizeof(hashtable_Key)) == 0;
}

// data を破棄する
// 引数
// data: 破棄するデータ
// 返値
// なし
void hashtable_destroyData_private(void *data)
{
    // 今回は data の中にポインタは無いので、データを破棄するだけ
    free(data);
}

// key を破棄する
// 引数
// key: 破棄するキー
// 返値
// なし
void hashtable_destroyKey_private(void *key)
{
    // 今回は data の中にポインタは無いので、データを破棄するだけ
    free(key);
}

// private

// キーが存在するか確認して、当該 cell を返す
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 探すデータのキー
// 返値
// 当該キーが含まれる cell のポインタ
hashtable_Cell *hashtable_find_private(hashtable_Context *context, void *key)
{
    hashtable_Cell *cell = context->table[(*context->hash)(key)];
    
    // キーが同じ cell を探し、見つかればその cell を返す
    while (cell != NULL) {
        if ((*context->compareKey)(cell->key, key)) { return cell; }
        cell = cell->next;
    }
    
    // 存在しなかった場合は NULL を返す
    return NULL;
}

// ハッシュテーブルに対してデータを削除する
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 削除するデータのキー
// 返値
// なし
void hashtable_delete_private(hashtable_Context *context, void *key)
{
    hashtable_Cell *cell, *previousCell;
    
    // 見つからない場合は抜ける
    if (context->table[(*context->hash)(key)] == NULL) { return; }
    
    // テーブルの先頭から探して、ポインタの付け替えを行う
    for (cell = context->table[(*context->hash)(key)], previousCell = NULL; cell != NULL; previousCell = cell, cell = cell->next) {
        if ((*context->compareKey)(cell->key, key)) {
            // 先頭の場合
            if (previousCell == NULL) { context->table[(*context->hash)(key)] = cell->next; }
            // それ以外の場合
            else { previousCell->next = cell->next; }
            (*context->destroyData)(cell->data);
            (*context->destroyKey)(cell->key);
            free(cell);
            
            return;
        }
    }
}

// public
// ハッシュテーブルを初期化する
// 引数
// context: ハッシュテーブルのコンテキスト
// tableSize: ハッシュテーブルのサイズ
// hash(): ハッシュ計算を行う関数
// compareKey(): キーが一致しているかどうか確認する関数
// destroyData(): データを破棄する際の関数
// destoryKey(): キーを破棄する際の関数
// 返値
// なし
void hashtable_initialize(hashtable_Context *context, int tableSize, int (*hash)(void *key), int (*compareKey)(void *key1, void *key2), void (*destroyData)(void *data), void (*destoryKey)(void *key))
{
    int i;
    
    // 値を初期化
    context->tableSize = tableSize;
    context->i = 0;
    context->currentCell = NULL;
    // 関数(のポインタ)を初期化
    context->hash = hash;
    context->compareKey = compareKey;
    context->destroyData = destroyData;
    context->destroyKey = destoryKey;
    // mutex 初期化 (第二引数が NULL はデフォルトの設定で mutex を生成するという意味)
    pthread_mutex_init(&context->mutex, NULL);
    
    // テーブルの領域を確保
    if ((context->table = (hashtable_Cell **)malloc(sizeof(hashtable_Cell) * tableSize)) == NULL) { perror("initialize: malloc() error."); }
    
    // テーブルの中身を全て NULL にする
    for (i = 0; i < context->tableSize; i++) { context->table[i] = NULL; }
}

// キーが存在するか確認して、当該 cell を返す (hashtable_find_private のラッパー関数でロックあり)
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 探すデータのキー
// 返値
// 当該キーが含まれる cell のポインタ
hashtable_Cell *hashtable_find(hashtable_Context *context, void *key)
{
    hashtable_Cell *cell;
    
    // ロック
    if (pthread_mutex_lock(&context->mutex) != 0) { perror("find: fail mutex lock."); }
    
    cell = hashtable_find_private(context, key);
    
    // ロックを解除して終了
    if (pthread_mutex_unlock(&context->mutex) != 0) { perror("find: fail mutex unlock."); }
    
    return cell;
}

// ハッシュテーブルに対してデータを挿入する
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 挿入するデータのキー
// data: 挿入するデータ
// 返値
// 挿入したか、破棄したか
int hashtable_insert(hashtable_Context *context, void *key, void *data)
{
    hashtable_Cell *cell, *head;
    
    // ロック
    if (pthread_mutex_lock(&context->mutex) != 0) { perror("insert: fail mutex lock."); }
    
    // 既に存在するものだったので何もしない
    if (hashtable_find_private(context, key) != NULL) {
        // ロックを解除して終了
        if (pthread_mutex_unlock(&context->mutex) != 0) { perror("insert: fail mutex unlock."); }
        
        return hashtable_EXIST;
    }
    
    // 同じハッシュ値のテーブルの先頭の物をもらってくる
    head = context->table[(*context->hash)(key)];
    
    // 新しい cell を作る
    if ((cell = (hashtable_Cell *)malloc(sizeof(hashtable_Cell))) == NULL) { perror("insert: malloc() error."); }
    cell->key = key;
    cell->data = data;
    cell->next = head;
    
    // 先頭を付け替える
    context->table[(*context->hash)(key)] = cell;
    
    // ロックを解除して終了
    if (pthread_mutex_unlock(&context->mutex) != 0) { perror("insert: fail mutex unlock."); }
    
    return hashtable_NOT_EXIST;
}

// ハッシュテーブルに対してデータを更新する
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 更新するデータのキー
// data: 更新するデータ
// 返値
// なし
void hashtable_update(hashtable_Context *context, void *key, void *data)
{
    hashtable_Cell *cell;
    
    // ロック
    if (pthread_mutex_lock(&context->mutex)) { perror("update: fail mutex lock."); }
    
    // 更新するデータを取得(見つからなかった場合は抜ける)
    if ((cell = hashtable_find_private(context, key)) == NULL) {
        // ロックを解除して抜ける
        if (pthread_mutex_unlock(&context->mutex)) { perror("update: fail mutex unlock."); }
        return;
    }
    
    // データを更新
    cell->data = data;
    
    // ロックを解除
    if (pthread_mutex_unlock(&context->mutex)) { perror("update: fail mutex unlock."); }
}

// ハッシュテーブルに対してデータを削除する (hashtable_delete_private のラッパー関数でロックあり)
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 削除するデータのキー
// 返値
// なし
void hashtable_delete(hashtable_Context *context, void *key)
{
    // ロック
    if (pthread_mutex_lock(&context->mutex)) { perror("delete: fail mutex lock."); }
    
    hashtable_delete_private(context, key);
    
    // ロックを解除
    if (pthread_mutex_unlock(&context->mutex)) { perror("delete: fail mutex unlock."); }
}

// キーに応じたデータを返す
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 手に入れるデータのキー
// 返値
// 手に入れたデータ
void *hashtable_get(hashtable_Context *context, void *key)
{
    hashtable_Cell *cell;
    
    // ロック
    if (pthread_mutex_lock(&context->mutex)) { perror("get: fail mutex lock."); }
    
    // cell を探してくる
    cell = hashtable_find_private(context, key);
    
    // ロックを解除
    if (pthread_mutex_unlock(&context->mutex)) { perror("get: fail mutex unlock."); }
    
    if (cell != NULL) { return cell->data; }
    else { return NULL; }
}

// ハッシュテーブルを総なめするときに使う関数
// 引数
// なし
// 返値
// ハッシュテーブルの要素
hashtable_Cell *hashtable_next(hashtable_Context *context)
{
    //static hashtable_Cell *cell = NULL;
    hashtable_Cell *returnCell;
    
    // ロック
    if (pthread_mutex_lock(&context->mutex)) { perror("next: fail mutex lock."); }
    
    // 線形リストの要素がある場合はそこからデータ取りだし
    if (context->currentCell != NULL) {
        //*key = cell->key;
        //*data = cell->data;
        returnCell = context->currentCell;
        context->currentCell = context->currentCell->next;
        
        // ロックを解除
        if (pthread_mutex_unlock(&context->mutex)) { perror("next: fail mutex unlock."); }
        
        return returnCell;
    }
    // 線形リストに要素がない場合はテーブルの次を探す
    else { context->i += 1; }
    
    // テーブルからデータを探す
    for ( ; context->i < context->tableSize; context->i += 1) {
        for (context->currentCell = context->table[context->i]; context->currentCell != NULL; context->currentCell = context->currentCell->next) {
            if (context->currentCell != NULL) {
                //key = &cell->key;
                //data = &cell->data;
                returnCell = context->currentCell;
                context->currentCell = context->currentCell->next;
                
                // ロックを解除
                if (pthread_mutex_unlock(&context->mutex)) { perror("next: fail mutex unlock."); }
                
                return returnCell;
            }
        }
    }
    
    // 一周したので、 i を初期化
    context->i = 0;
    
    // ロックを解除
    if (pthread_mutex_unlock(&context->mutex)) { perror("next: fail mutex unlock."); }
    
    return NULL;
}

// 新しいデータを作成する(全て空)
// 引数
// data: データをセットする変数
// 返値
// なし
void hashtable_initializeData(hashtable_Data *data)
{
    data->state = 0;
    data->domain[0] = '\0';
    gettimeofday(&data->time, NULL);
}



// ------------------テスト用関数

void hashtable_hash_TEST(char *ip)
{
    hashtable_Key key;
    
    inet_aton(ip, &key.addr);
    
    printf("%d\n", hashtable_hash_private(&key));
}

void hashtable_print(hashtable_Data data)
{
    printf("%d\n", data.state);
    printf("%s\n", data.domain);
    // テストする上で時刻は毎回変化するので表示しない
    // printf("%ld.%d\n", data.time.tv_sec, data.time.tv_usec);
}

void hashtable_initializeData_TEST()
{
    hashtable_Data data;
    
    hashtable_initializeData(&data);
    hashtable_print(data);
}

// ファイルからテスト用のデータを読み込むようにする
// insert, delete などハッシュテーブルの操作に関する関数群のテスト用関数
void hashtable_operateHashtable_TEST(const char *filepath, hashtable_Context *context)
{
    char mode;
    char line[1024];
    int count;
    
    hashtable_Data *data;
    hashtable_Data *pdata;
    char ip[16];
    int state;
    char domain[256];
    
    hashtable_Key *key;
    hashtable_Cell *cell;
    
    FILE *file;
    
    file = fopen(filepath, "r");
        
    // 2パターンの入力ケースがあり、
    // ひとつはデータを取り出したりする
    // もうひとつはキーの取り出しに関する操作
    while (fgets(line, 1024, file) != NULL) {
        // mode がない場合(空白行)次の行へ
        if (sscanf(line, "%c", &mode) < 1) { continue; }
        // コメント行なので次の行へ
        if (mode == '#') { continue; }
        // 改行
        else if (mode == '\n') {
            printf("\n");
            continue;
        }
        else if (mode == 'c') {
            printf("%s", line);
            continue;
        }
        
        data = (hashtable_Data *)malloc(sizeof(hashtable_Data));
        // 初期化
        data->state = 0;
        data->domain[0] = '\0';
        gettimeofday(&data->time, NULL);
        
        key = (hashtable_Key *)malloc(sizeof(hashtable_Key));
        
        count = sscanf(line, "%c %s %d %s", &mode, ip, &state, domain);
        // コレで4の場合は domain state ip といくはず
        switch (count) {
            case 4:
                strncpy(data->domain, domain, 256);
            case 3:
                data->state = state;
            case 2:
                //inet_aton(ip, &data.ip);
                inet_aton(ip, &key->addr);
                break;
            case 1:
                break;
            default:
                break;
        }
        
        // mode によって関数呼び分け
        switch (mode) {
                // insert
            case 'i':
                if (count < 2) { continue; }
                if (hashtable_insert(context, (void *)key, (void *)data) == hashtable_EXIST) {
                    printf("key conflict %s", inet_ntoa(key->addr));
                }
                break;
            case 'u':
                if (count < 2) { continue; }
                hashtable_update(context, (void *)key, (void *)data);
                // delete
            case 'd':
                if (count < 2) { continue; }
                hashtable_delete(context, (void *)key);
                break;
                // search
            case 'f':
                if (count < 2) { continue; }
                cell = hashtable_find_private(context, (void *)key);
                if (cell == NULL) { printf("not found.\n"); }
                else { printf("found\n"); }
                break;
                // get
            case 'g':
                if (count < 2) { continue; }
                pdata = (hashtable_Data *)hashtable_get(context, (void *)key);
                //data = pdata;
                if (pdata == NULL) { printf("NULL\n"); }
                else { hashtable_print(*pdata); }
                break;
                // expire
            case 'e':
                //hashtable_expire(context);
                break;
                // 設定時刻を変更する
            case 't':
                //if (count < 3) { continue; }
                //cell = hashtable_find(hashtable_table, &hashtable_mutex, key);
                //cell->data.time.tv_sec += state;
                break;
            default:
                // テストケースが間違えているので入力行を表示する
                fprintf(stderr, "Invalid testcase. \"%s\"\n", line);
                break;
        }
    }
}

void hashtable_TEST(int argc, const char *argv[])
{
    int testCase;
    //struct timeval time;
    
    //hashtable_Cell *hashtable_table[hashtable_TABLE_MAXSIZE];
    //pthread_mutex_t hashtable_mutex;
    
    hashtable_Context hashtable_context;
    
    if (argc < 2) {
        fprintf(stderr, "plese input test case number.\n");
        exit(1);
    }
    
    testCase = atoi(argv[1]);
    hashtable_initialize(&hashtable_context, hashtable_TABLE_MAXSIZE, hashtable_hash_private, hashtable_compareKey_private, hashtable_destroyData_private, hashtable_destroyKey_private);
    
    switch (testCase) {
        case 0:
            // hashtable_hash() のテスト
            hashtable_hash_TEST("8.43.8.43");
            hashtable_hash_TEST("56.51.56.51");
            hashtable_hash_TEST("255.255.0.0");
            hashtable_hash_TEST("62.77.13.8");
            hashtable_hash_TEST("255.255.0.0");
            hashtable_hash_TEST("128.128.127.127");
            hashtable_hash_TEST("63.63.192.192");
            break;
        case 1:
            /*
            // hashtable_timevalToUnsignedInt() のテスト
            time.tv_sec = 10;
            time.tv_usec = 100;
            hashtable_timevalToUnsignedInt_TEST(time);
            time.tv_sec = 54;
            time.tv_usec = 825;
            hashtable_timevalToUnsignedInt_TEST(time);
            time.tv_sec = 982;
            time.tv_usec = 1476;
            hashtable_timevalToUnsignedInt_TEST(time);
             */
            break;
        case 2:
            // hashtable_initializeData() のテスト
            hashtable_initializeData_TEST();
            break;
        case 3:
            // ハッシュテーブル操作系のテスト
            hashtable_operateHashtable_TEST(argv[2], &hashtable_context);
            break;
        default:
            break;
    }
}


int main(int argc, const char * argv[])
{
    hashtable_TEST(argc, argv);
    //hashtable_main(argc, argv);
    
    return 0;
}

