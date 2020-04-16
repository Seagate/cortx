Summary:        Google C++ testing framework
Name:           gtest
Version:        1.7.0
Release:        1%{?dist}
License:        BSD
Group:          Development/Tools
URL:            https://github.com/google/googletest
Source:         %{name}-%{version}.tar.gz
BuildRequires:  python36 cmake libtool
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

%description
Google's framework for writing C++ tests on a variety of platforms
(GNU/Linux, Mac OS X, Windows, Windows CE, and Symbian). Based on the
xUnit architecture. Supports automatic test discovery, a rich set of
assertions, user-defined assertions, death tests, fatal and non-fatal
failures, various options for running the tests, and XML test report
generation.

# below definition needed, without that on RHEL 8 seeing failure
# error: Empty %files file /root/rpmbuild/BUILD/gmock-1.7.0/debugsourcefiles.list
%global debug_package %{nil}

%package        devel
Summary:        Development files for %{name}
Group:          Development/Libraries
Requires:       automake
Requires:       %{name} = %{version}-%{release}

%description    devel
This package contains development files for %{name}.

%prep
%setup -q

%build
mkdir build
cd build
CFLAGS=-fPIC CXXFLAGS=-fPIC cmake ..

make %{?_smp_mflags}

%check
# do nothing

%install
rm -rf %{buildroot}
# make install doesn't work anymore.
# need to install them manually.
install -d $RPM_BUILD_ROOT{%{_bindir},%{_datadir}/aclocal,%{_includedir}/gtest{,/internal},%{_libdir}}
# just for backward compatibility
install -p -m 0755 build/libgtest.a build/libgtest_main.a $RPM_BUILD_ROOT%{_libdir}/
/sbin/ldconfig -n $RPM_BUILD_ROOT%{_libdir}
install -p -m 0644 include/gtest/*.h $RPM_BUILD_ROOT%{_includedir}/gtest/
install -p -m 0644 include/gtest/internal/*.h $RPM_BUILD_ROOT%{_includedir}/gtest/internal/

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root, -)
%doc CHANGES CONTRIBUTORS LICENSE README
%{_libdir}/libgtest.a
%{_libdir}/libgtest_main.a

%files devel
%defattr(-, root, root, -)
%doc samples
%{_libdir}/libgtest.a
%{_libdir}/libgtest_main.a
%{_includedir}/gtest

%changelog
# Refer https://github.com/google/googletest/blob/release-%{version}/CHANGES

