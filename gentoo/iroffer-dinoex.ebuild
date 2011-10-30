# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header$

DESCRIPTION="An improved version of iroffer - an IRC XDCC bot."
HOMEPAGE="http://iroffer.dinoex.net"
SRC_URI="http://iroffer.dinoex.net/iroffer-dinoex-snap.tar.gz"
KEYWORDS="~x86 ~amd64"
LICENSE="GPL-2"
SLOT="0"
LANGUAGES="linguas_fr linguas_it linguas_de linguas_en"
IUSE="geoip curl tls upnp ruby +blowfish +openssl +http +admin +telnet +memsave static ${LANGUAGES}"

DEPEND="ruby? ( dev-lang/ruby )
    geoip? ( dev-libs/geoip )
    curl? ( net-misc/curl )
    tls? ( net-libs/gnutls )
    openssl? ( dev-libs/openssl )"
RDEPEND="!static? ( ${DEPEND} )"


src_compile() {
    local configure_opts

    if use geoip ; then
        configure_opts="${configure_opts} -geoip"
    fi

    if use curl ; then
        configure_opts="${configure_opts} -curl"
    fi

    if use tls ; then
        if use openssl ; then
            die "Please select only openssl or tls!"
        fi

        configure_opts="${configure_opts} -tls"
    fi

    if use upnp ; then
        configure_opts="${configure_opts} -upnp"
    fi

    if use ruby ; then
        configure_opts="${configure_opts} -ruby"
    fi

    if ! use blowfish ; then
        configure_opts="${configure_opts} -no-blowfish"
    fi

    if ! use openssl ; then
        configure_opts="${configure_opts} -no-openssl"
    fi

    if ! use http ; then
        configure_opts="${configure_opts} -no-http"
    fi

    if ! use admin ; then
        configure_opts="${configure_opts} -no-admin"
    fi

    if ! use telnet ; then
        configure_opts="${configure_opts} -no-telnet"
    fi

    if ! use memsave ; then
        configure_opts="${configure_opts} -no-memsave"
    fi

    if use static ; then
        configure_opts="${configure_opts} -no-libs"
    fi

    echo ${configure_opts}
    ./Configure ${configure_opts}

    if use linguas_en ; then
        ./Lang en
    elif use linguas_de ; then
        ./Lang de
    elif use linguas_it ; then
        ./Lang it
    elif use linguas_fr ; then
        ./Lang fr
    fi

    emake || die "emake failed"
}

src_install() {
    dobin iroffer

    insinto /usr/share/iroffer-dinoex/
    doins sample.config
    doins beispiel.config
    doins exemple.config
    doins iroffer.cron

    if use ruby ; then
        doins ruby-sample.rb
    fi

    if use http ; then
        doins *.html

        insinto /usr/share/iroffer-dinoex/htocs/
        doins htdocs/*
    fi

    dodoc LICENSE
    dodoc LIESMICH.modDinoex
    dodoc README
    dodoc README.modDinoex
    dodoc THANKS
    dodoc TODO

    if use admin ; then
        if use linguas_en ; then
            dodoc help-admin-en.txt
        fi

        if use linguas_de ; then
            dodoc help-admin-de.txt
        fi

        if use linguas_it ; then
            dodoc help-admin-it.txt
        fi

        if use linguas_fr ; then
            dodoc help-admin-fr.txt
        fi
    fi
   
    doman iroffer.1
    doman xdcc.7
}

