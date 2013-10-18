#ifndef hashtable_hashtable_h
#define hashtable_hashtable_h

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>

// 定数
// ハッシュテーブルの大きさ
// 2の16乗
#define hashtable_TABLE_MAXSIZE 65536
#define hashtable_TIMESPAN_SEC (60 * 60 * 24)
#define hashtable_TIMESPAN_USEC 0

// insert の返値
enum {
    hashtable_NOT_EXIST = 0,
    hashtable_EXIST
};

// -----------------------------
// データ
struct __hashtable_Data {
    int state;
    struct timeval time;
    char domain[256];
};
typedef struct __hashtable_Data hashtable_Data;

// ハッシュテーブルのキー
struct __hashtable_Key {
    struct in_addr addr;
};
typedef struct __hashtable_Key hashtable_Key;

// ハッシュテーブルの中身
struct __hashtable_cell {
    void *key;
    void *data;
    struct __hashtable_cell *next;
};
typedef struct __hashtable_cell hashtable_Cell;

// ハッシュテーブルのコンテキスト(ハッシュテーブル実現に必要なデータ群)
struct __hashtable_context {
    hashtable_Cell **table;
    int i;
    hashtable_Cell *currentCell;
    int tableSize;
    pthread_mutex_t mutex;
    int (*hash)();
    int (*compareKey)();
    void (*destroyData)();
    void (*destroyKey)();
};
typedef struct __hashtable_context hashtable_Context;

// -----------------------------

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
void hashtable_initialize(hashtable_Context *context, int tableSize, int (*hash)(void *key), int (*compareKey)(void *key1, void *key2), void (*destroyData)(void *data), void (*destoryKey)(void *key));

// キーが存在するか確認して、当該 cell を返す (hashtable_find_private のラッパー関数でロックあり)
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 探すデータのキー
// 返値
// 当該キーが含まれる cell のポインタ
hashtable_Cell *hashtable_find(hashtable_Context *context, void *key);

// ハッシュテーブルに対してデータを挿入する
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 挿入するデータのキー
// data: 挿入するデータ
// 返値
// 挿入したか、破棄したか
int hashtable_insert(hashtable_Context *context, void *key, void *data);

// ハッシュテーブルに対してデータを更新する
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 更新するデータのキー
// data: 更新するデータ
// 返値
// なし
void hashtable_update(hashtable_Context *context, void *key, void *data);

// ハッシュテーブルに対してデータを削除する (hashtable_delete_private のラッパー関数でロックあり)
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 削除するデータのキー
// 返値
// なし
void hashtable_delete(hashtable_Context *context, void *key);

// キーに応じたデータを返す
// 引数
// context: ハッシュテーブルのコンテキスト
// key: 手に入れるデータのキー
// 返値
// 手に入れたデータ
void *hashtable_get(hashtable_Context *context, void *key);

// ハッシュテーブルを総なめするときに使う関数
// 引数
// なし
// 返値
// ハッシュテーブルの要素
hashtable_Cell *hashtable_next(hashtable_Context *context);

// 新しいデータを作成する(全て空)
// 引数
// data: データをセットする変数
// 返値
// なし
void hashtable_initializeData(hashtable_Data *data);



#endif
