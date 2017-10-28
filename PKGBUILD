pkgname=loot
pkgver=1.0
pkgrel=1
arch=('i686' 'x86_64')
depends=('gtk3')

build() {
	make -C "${startdir}"
}

package() {
	make -C "${startdir}" DESTDIR="${pkgdir}" install

	install -D -m644 "${startdir}/examples/${pkgname}.desktop" \
		"${pkgdir}/usr/share/applications/${pkgname}.desktop"

	install -D -m644 "${startdir}/icons/box-opened.svg" \
		"${pkgdir}/usr/share/pixmaps/${pkgname}.svg"
}
