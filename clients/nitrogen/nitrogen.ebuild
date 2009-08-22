# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /phenix/u/hpereira/repository/kde/kde4-windeco-nitrogen/nitrogen.ebuild,v 1.1 2009/05/26 21:55:20 hpereira Exp $

EAPI="2"

inherit kde4-base

MY_P="kde4-windeco-${P}-Source"

DESCRIPTION="A window decoration for KDE 4.2.x"
HOMEPAGE="http://www.kde-look.org/content/show.php/Nitrogen?content=99551"
SRC_URI="http://www.kde-look.org/CONTENT/content-files/99551-${MY_P}.tar.gz"

S="${WORKDIR}/${MY_P}"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="debug"
