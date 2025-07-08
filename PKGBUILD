pkgname=loot
pkgver=1.1
pkgrel=1
arch=('i686' 'x86_64')
depends=('gtk3')

build() {
    make -C "${startdir}" ICONS_PREFIX="/usr/share/${pkgname}/icons"
}

package() {
    mkdir -p "${pkgdir}/usr/bin"
    install "${startdir}/loot" "${pkgdir}/usr/bin"

    mkdir -p "${pkgdir}/usr/share/${pkgname}/icons"
    install -m644 "${startdir}/icons/"*.png "${pkgdir}/usr/share/${pkgname}/icons"

    mkdir -p "${pkgdir}/usr/share/applications"
    install -m644 "${startdir}/examples/${pkgname}.desktop" \
        "${pkgdir}/usr/share/applications/${pkgname}.desktop"

    mkdir -p "${pkgdir}/usr/share/icons/hicolor/scalable/apps"
    install -m644 "${startdir}/icons/box-opened.svg" \
        "${pkgdir}/usr/share/icons/hicolor/scalable/apps/${pkgname}.svg"
}
