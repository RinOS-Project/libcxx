# libc++ TODO

- [ ] **LIBCXX-CONDITION-VARIABLE-REGISTRY-CAPACITY-01 / P2 / M** — **受入:** 128枠を超える同時生成・破棄、ページ追加の並行実行と割当失敗、close後の再利用・generation更新、wait／notify／破棄競合をIA-32／x86_64で実行確認する。runtime受入未実施のため未完了とする。
- [ ] **LIBCXX-CHRONO-TZDB-01 / P2 / L** — **source確認 (2026-11-02):** `<chrono>`にはcalendar型はあるが、C++20の`time_zone`／`tzdb`／`zoned_time`、`locate_zone()`、`current_zone()`がなく、`version.h`もこの欠落を理由に`__cpp_lib_chrono`を201611Lに留める。**受入:** システムのtimezone databaseを取得・更新するproviderとzone名検索を用意し、UTC／local変換、DST遷移、存在しない時刻・重複時刻の選択、timezone databaseの再読込を標準APIから扱えるようにする。zone data欠損時のエラーを定義し、実装後にfeature-test macroを対応revisionへ更新する。
- [ ] **LIBCXX-LOCALE-CATALOG-01 / P2 / M** — **受入:** locale名と別名の正規化、数値・通貨カテゴリの合成、char／wcharのnamed facetにおける記号・桁区切り・符号位置をIA-32／x86_64で確認する。環境変数によるカテゴリ別選択、UTF-8区切り文字のchar表現、C locale APIとの整合性、存在しないlocaleの例外動作も確認する。
- [ ] **LIBCXX-NEXTAFTERL-ABI-01 / P2 / M** — `__nextafter.h`は未対応の`long double` ABIで`double`へ変換するため、拡張精度の隣接値を返せない。**受入:** 対応targetごとの表現またはABI builtinを実装し、拡張精度の1 ULP境界を維持する。
