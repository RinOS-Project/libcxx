/*
 * RinOS C++ Standard Library ✿
 * 統合ヘッダファイル
 * 
 * 使用方法:
 *   #include <libcxx/libcxx.h>
 * 
 * または個別に:
 *   #include <libcxx/vector.h>
 *   #include <libcxx/string.h>
 *   etc.
 */

#ifndef RINOS_LIBCXX_H
#define RINOS_LIBCXX_H

/* ═══════════════════════════════════════════════════════════════
 * RinOS C++ ライブラリ概要
 * 
 * このライブラリはRinOS用のfreestanding C++ STL実装です。
 * 標準ライブラリの主要機能をヘッダオンリーで提供します。
 * 
 * 特徴:
 * - 例外なし（-fno-exceptions対応）
 * - RTTI不要（-fno-rtti対応）
 * - ホスト標準ライブラリ依存なし
 * - STL部分はヘッダオンリー
 * - C++ ABI symbolは製品app runtimeが提供
 * 
 * 必要な外部シンボル:
 * - rin_malloc(): メモリ確保
 * - rin_free(): メモリ解放
 * 
 * ビルド設定例:
 *   CXXFLAGS = -ffreestanding -fno-exceptions -fno-rtti \
 *              -nostdlib -I$(RINOS)/libs
 * ═══════════════════════════════════════════════════════════════*/

/* C++ ランタイム基盤 */
#include "rincxx.h"

/* 型特性 */
#include "type_traits.h"

/* コンテナ */
#include "array.h"
#include "vector.h"
#include "string.h"
#include "map.h"

/* メモリ管理 */
#include "memory.h"

/* ユーティリティ */
#include "optional.h"
#include "tuple.h"
#include "functional.h"
#include "initializer_list.h"
#if defined(__cplusplus) && __cplusplus > 202002L
#include "stacktrace.h"
#endif

/* アルゴリズム */
#include "algorithm.h"

/* ストリーム */
#include "sstream.h"

/* 例外型とbounded runtime boundary */
#include "exception.h"

/* 時間 */
#include "ctime.h"

/* 標準ライブラリ関数 */
#include "cstdlib.h"

/* 文字分類 */
#include "cctype.h"

/* イテレータ */
#include "iterator.h"

/* ═══════════════════════════════════════════════════════════════
 * バージョン情報
 * ═══════════════════════════════════════════════════════════════*/

#define RINOS_LIBCXX_VERSION_MAJOR 1
#define RINOS_LIBCXX_VERSION_MINOR 0
#define RINOS_LIBCXX_VERSION_PATCH 0
#define RINOS_LIBCXX_VERSION "1.0.0"

/* C++標準バージョン近似 */
#if __cplusplus >= 202002L
#define RINOS_CXX20 1
#endif
#if __cplusplus >= 201703L
#define RINOS_CXX17 1
#endif
#if __cplusplus >= 201402L
#define RINOS_CXX14 1
#endif
#if __cplusplus >= 201103L
#define RINOS_CXX11 1
#endif

#endif /* RINOS_LIBCXX_H */
