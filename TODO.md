# libc++ TODO

- [ ] **LIBCXX-LOCALE-CATALOG-01 / P2 / M** — `locale.h`のnamed locale facetは組み込みprofileのみで、インストール済みlocale catalogから任意のlocale dataを取得できない。**受入:** catalog providerからlocaleを解決し、欠損・不正localeのfallbackを定義する。
- [ ] **LIBCXX-NEXTAFTERL-ABI-01 / P2 / M** — `__nextafter.h`は未対応の`long double` ABIで`double`へ変換するため、拡張精度の隣接値を返せない。**受入:** 対応targetごとの表現またはABI builtinを実装し、拡張精度の1 ULP境界を維持する。
