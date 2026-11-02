# libc++ TODO

- [ ] **LIBCXX-CHRONO-TZDB-01 / P2 / L** — **source確認 (2026-11-02):** `<chrono>`にはcalendar型はあるが、C++20の`time_zone`／`tzdb`／`zoned_time`、`locate_zone()`、`current_zone()`がなく、`version.h`もこの欠落を理由に`__cpp_lib_chrono`を201611Lに留める。**受入:** システムのtimezone databaseを取得・更新するproviderとzone名検索を用意し、UTC／local変換、DST遷移、存在しない時刻・重複時刻の選択、timezone databaseの再読込を標準APIから扱えるようにする。zone data欠損時のエラーを定義し、実装後にfeature-test macroを対応revisionへ更新する。
- [ ] **LIBCXX-LOCALE-CATALOG-01 / P2 / M** — `locale.h`のnamed locale facetは組み込みprofileのみで、インストール済みlocale catalogから任意のlocale dataを取得できない。**受入:** catalog providerからlocaleを解決し、欠損・不正localeのfallbackを定義する。
- [ ] **LIBCXX-NEXTAFTERL-ABI-01 / P2 / M** — `__nextafter.h`は未対応の`long double` ABIで`double`へ変換するため、拡張精度の隣接値を返せない。**受入:** 対応targetごとの表現またはABI builtinを実装し、拡張精度の1 ULP境界を維持する。
