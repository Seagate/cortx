Name: log4cxx_eos
Version: 0.10.0
Release: 1

Summary: A logging framework for C++ patterned after Apache log4j
License: Apache
Group: Development/Libraries/C and C++
Url: http://logging.apache.org/log4cxx/

Source: apache-log4cxx-%{version}.tar.bz2
Patch0: apache-log4cxx-%{version}.patch
Prefix: %_prefix
BuildRoot: %{_tmppath}/apache-log4cxx-%{version}-build
#BuildRequires: gcc-c++  gcc-fortran
Requires: unixODBC

%description
Apache log4cxx is a logging framework for C++ patterned after Apache log4j.
Apache log4cxx uses Apache Portable Runtime for most platform-specific code
and should be usable on any platform supported by APR.

%package -n log4cxx_eos-devel
License:        Apache
Group:          Development/Libraries/C and C++
Requires:       %{name} = %{version}
Summary:        The development files for log4cxx
#Requires:	unixODBC-devel libapr-util1-devel openldap2-devel libdb-4_5 libexpat-devel libapr1-devel glibc-devel

%description -n log4cxx_eos-devel
Apache log4cxx is a logging framework for C++ patterned after Apache log4j.
Apache log4cxx uses Apache Portable Runtime for most platform-specific code
and should be usable on any platform supported by APR.

%prep
%setup -n apache-log4cxx-%{version}
%patch0 -p1

%build
./autogen.sh
./configure --prefix=/usr --libdir=%{_libdir}
make

%install
make install prefix=$RPM_BUILD_ROOT%{prefix} libdir=$RPM_BUILD_ROOT%{_libdir}

# files
cd $RPM_BUILD_ROOT
find .%{_includedir}/log4cxx -print | sed 's,^\.,\%attr(-\,root\,root) ,' >  $RPM_BUILD_DIR/file.list
find .%{_datadir}/log4cxx    -print | sed 's,^\.,\%attr(-\,root\,root) ,' >> $RPM_BUILD_DIR/file.list


%clean
rm -rf $RPM_BUILD_ROOT
rm -rf $RPM_BUILD_DIR/%{name}-%{version}
rm $RPM_BUILD_DIR/file.list

%post   -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%{_libdir}/liblog4cxx.so.10
%{_libdir}/liblog4cxx.so.10.0.0

%files -n log4cxx_eos-devel -f ../file.list
%defattr(-,root,root,-)
%{_libdir}/liblog4cxx.a
%{_libdir}/liblog4cxx.la
%{_libdir}/liblog4cxx.so
%{_libdir}/pkgconfig/liblog4cxx.pc
