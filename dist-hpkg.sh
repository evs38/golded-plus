#!/bin/sh
# $Id$
# Create a Haiku package 'golded_plus-1.1.5.YMMDD-1-<arch>.hpkg'.
#
# Run it on Haiku, from the top of the source tree. It builds the BeOS
# target - which is what Haiku is, as far as this source is concerned -
# and wraps the result the way Haiku expects.
#
# The headers for ncurses and iconv come from HaikuPorts:
#
#     pkgman install ncurses6_devel libiconv_devel
#
# and Haiku mounts them under /boot/system/develop/headers only after a
# reboot. Before that they simply are not there and the build falls back
# to the compiled-in charset tables.

srcdatefile=srcdate.h
build=`sed -n 's/.*"\([[:digit:]]\{8\}\)".*/\1/p' $srcdatefile`

platform="be"
binsuffix="be"
binesdir="bin"
stage="${binesdir}/hpkg"

arch=`uname -m`
case "$arch" in
  BePC|i586|i686) arch=x86 ;;
esac

version="1.1.5.${build}"
release="1"
pkgname="golded_plus-${version}-${release}-${arch}.hpkg"
bines="${binesdir}/ged${binsuffix} ${binesdir}/gn${binsuffix} ${binesdir}/rddt${binsuffix}"

echo "Build a GoldED+/Haiku package: ${pkgname}"

if [ ! -f golded3/mygolded.h ]; then
  cp golded3/mygolded.__h golded3/mygolded.h
  echo "golded3/mygolded.h is created now. Please edit this file"
  exit 1
fi

if [ ! -d "${binesdir}" ] ; then mkdir ${binesdir}; fi

# make binaries

make PLATFORM=${platform} clean all strip || { echo "Build failed"; exit 1; }

for i in ${bines} ; do
  if [ ! -f ${i} ] ; then echo "File ${i} not exists, stop!"; exit 1 ; fi
done

# stage the files in the layout a package is unpacked into

rm -rf ${stage}
mkdir -p ${stage}/bin
mkdir -p ${stage}/documentation/man/man1
mkdir -p ${stage}/documentation/packages/golded_plus
mkdir -p ${stage}/data/golded+/cfgs
mkdir -p ${stage}/data/golded+/charset

cp ${binesdir}/ged${binsuffix}  ${stage}/bin/golded
cp ${binesdir}/gn${binsuffix}   ${stage}/bin/goldnode
cp ${binesdir}/rddt${binsuffix} ${stage}/bin/rddt

for i in docs/*.1 ; do
  [ -f "$i" ] && cp "$i" ${stage}/documentation/man/man1/
done

for i in docs/copying docs/copying.lib docs/notework.txt docs/todowork.txt \
         docs/tips.txt docs/rusfaq.txt manuals/gold_ref.txt manuals/gold_usr.txt ; do
  [ -f "$i" ] && cp "$i" ${stage}/documentation/packages/golded_plus/
done

cp cfgs/config/*.cfg ${stage}/data/golded+/cfgs/ 2>/dev/null
cp cfgs/charset/*.chs ${stage}/data/golded+/charset/ 2>/dev/null

# the package description

cat > ${stage}/.PackageInfo <<__PACKAGEINFO_EOF__
name			golded_plus
version			${version}-${release}
architecture		${arch}
summary			"GoldED+ - an FTN message editor"
description		"GoldED+ is a message editor for FidoNet Technology \\
Networks. It reads and writes the JAM, Squish, *.MSG, Hudson, Ezycom, \\
PCBoard and Synchronet message bases, holds text in UTF-8 internally and \\
converts on the way to and from the wire."
packager		"GoldEd+ team <golded-plus@users.sourceforge.net>"
vendor			"GoldEd+ team"
licenses {
	"GNU GPL v2"
}
copyrights {
	"1990-2005 Odinn Sorensen, Alexander Aganichev, Jacobo Tarrio and others"
	"2005-2026 Stas Degteff and the GoldEd+ team"
}
provides {
	golded_plus = ${version}
	cmd:golded = ${version}
	cmd:goldnode = ${version}
	cmd:rddt = ${version}
}
requires {
	haiku
	lib:libncursesw
	lib:libiconv
}
__PACKAGEINFO_EOF__

# build it

rm -f ${binesdir}/${pkgname}
( cd ${stage} && package create -C . ../${pkgname} ) || {
	echo "package create failed"; exit 1; }

rm -rf ${stage}
echo "Created ${binesdir}/${pkgname}"
