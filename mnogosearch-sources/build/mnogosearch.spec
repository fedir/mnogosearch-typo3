# Build instructions:

# Requires static libraries:
# yum install glibc-static zlib-static
# MySQL: ./configure --disable-shared --enable-static --without-server
# PgSQL: ./configure --disable-shared
# rpmbuild -ba mnogosearch.spec
# Make sure the binaries are really static: "file indexer search.cgi"

%define	_version		3
%define	_release		3
%define _minor			14
%define _suffix			-%{_version}.%{_release}.%{_minor}
%define _pkg_release		01

%define _zlibdir		/usr
%define _mysqldir		/opt/mysql-static
%define _pgsqldir		/opt/pgsql-static
%define _strict_mysql_deps	0

%define _build_static	1

%{?_with_static: %{expand: %%define _build_static 1}}

%if %{_build_static}
%define _pkg_release_fin	%{_pkg_release}.static
%else
%define _pkg_release_fin	%{_pkg_release}
%endif

Summary:	Full-featured MySQL based web search engine.
Name: 		mnogosearch
Version: 	%{_version}.%{_release}.%{_minor}
Release: 	%{_pkg_release_fin}
License: 	GNU GPL Version 2
Group: 		Applications/Internet
URL: 		http://www.mnogosearch.org/
Source: 	http://www.mnogosearch.org/Download/mnogosearch-%{_version}.%{_release}.%{_minor}.tar.gz
#Patch0: 	mnogosearch-libs-3.2.39.patch
BuildRoot: 	%{_tmppath}/%{name}-%{version}-root
%if !%{_build_static}
Requires:	openssl, zlib
%endif
BuildRequires:	openssl-devel, zlib-devel
%if %{_strict_mysql_deps} && !%{_build_static}
Requires: 	MySQL-shared
%endif
%if %{_strict_mysql_deps}
BuildRequires:	MySQL-devel
%endif
%if %{_build_static}
BuildRequires:	pkgconfig
%endif


%description
mnoGoSearch is a full-featured MySQL based web search engine. mnoGoSearch consists of 
two parts. The first part is an indexing mechanism (indexer). The indexer walks over 
html hypertext references and stores found words and new references into a database. 
The second part is a web CGI front-end to provide search using data collected by the 
indexer. 

A PHP and a Perl front-ends are also available from our site http://www.mnogosearch.org/.
 
mnoGoSearch first release took place in November 1998. The search engine was named 
UDMSearch until the project was acquired by Lavtech.Com Corp. in October 2000 and 
its name changed to mnoGoSearch.

%package devel
Summary: 	Header files and libraries for developing apps which will use mnoGoSearch.
Group: 		Development/Libraries
%if !%{_build_static}
Requires: 	mnogosearch = %{version}, mnogosearch-libs = %{version}
Requires:	openssl, zlib
%endif
Requires: 	openssl-devel, zlib-devel
BuildRequires:	openssl-devel, zlib-devel

%if !%{_build_static} && %{_strict_mysql_deps}
Requires: 	MySQL-shared
%endif
%if %{_strict_mysql_deps}
BuildRequires:	MySQL-devel
%endif

%description devel

Header files, static and dynamic libraries with mnoGoSearch functions,
for developing applications which will use the mnoGoSearch engine.

%if !%{_build_static}
%package libs
Summary: 	Libraries for applications using mnoGoSearch
Group: 		System Environment/Libraries
Requires:	openssl, zlib
BuildRequires:	openssl-devel, zlib-devel
%endif

%if %{_strict_mysql_deps} && !%{_build_static}
Requires: 	MySQL-shared
BuildRequires:	MySQL-devel
%endif

%if !%{_build_static}
%description libs

Libraries for applications using the mnoGoSearch engine.
%endif

%prep
%setup -q 
#%patch0 -p1

%build

%if %{_build_static}
# FixMe: Remove call to pkg-config as soon as build will success without it.
# LIBS="`pkg-config openssl --libs`" \
CFLAGS="$RPM_OPT_FLAGS" \
./configure --prefix=%{_prefix} --exec-prefix=%{_exec_prefix} \
	    --datadir=%{_datadir}/mnogosearch \
	    --localstatedir=%{_localstatedir}/lib/mnogosearch \
	    --sysconfdir=%{_sysconfdir}/mnogosearch \
	    --libdir=%{_libdir} \
	    --infodir=%{_infodir} --mandir=%{_mandir} \
	    --with-zlib=%{_zlibdir} \
	    --with-mysql=%{_mysqldir} \
	    --with-pgsql=%{_pgsqldir} \
	    --with-extra-charsets=all \
	    --disable-syslog \
	    --disable-shared --enable-all-static
%else
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{_prefix} --exec-prefix=%{_exec_prefix} \
	    --datadir=%{_datadir}/mnogosearch%{_suffix} --localstatedir=%{_localstatedir}/mnogosearch%{_suffix} \
	    --sysconfdir=%{_sysconfdir}/mnogosearch%{_suffix} --infodir=%{_infodir} --mandir=%{_mandir} \
	    --with-openssl \
	    --with-zlib=%{_zlibdir} \
	    --with-mysql=%{_mysqldir} \
	    --disable-syslog \
	    --program-suffix=%{_suffix}
%endif
	    
make %{?_smp_mflags}

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir -p $RPM_BUILD_ROOT

DESTDIR="${RPM_BUILD_ROOT}" make install %{?_smp_mflags}
pushd ${RPM_BUILD_ROOT}/%{_prefix}
#mv doc ${RPM_BUILD_ROOT}/%{_datadir}/mnogosearch
rm -rf share/doc
popd

%if !%{_build_static}

%post libs 
/sbin/ldconfig

%postun libs 
/sbin/ldconfig

%post devel 
/sbin/ldconfig

%postun devel  
/sbin/ldconfig

%endif

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc AUTHORS COPYING ChangeLog INSTALL NEWS README
%doc %{_mandir}/*/*
%{_datadir}/mnogosearch
%{_localstatedir}/lib/mnogosearch
%{_sysconfdir}/mnogosearch
%{_bindir}/*
%{_sbindir}/*

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/*.a
%{_libdir}/*.la
%if !%{_build_static}
%{_libdir}/*.so
%endif

%if !%{_build_static}
%files libs
%defattr(-,root,root)
%{_libdir}/*.so
%endif

%changelog
* Thu Apr 01 2013 Alexander Barkov
  Updated to vetsion 3.3.14
* Thu Feb 28 2012 Alexander Barkov
  Updated to vetsion 3.3.13
* Tue Nov 23 2010 Alexander Barkov
  Updated to version 3.3.10.
