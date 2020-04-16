%global _enable_debug_package 0
%global debug_package %{nil}
%global __os_install_post /usr/lib/rpm/brp-compress %{nil}

Name:       bazel
Version:    0.13.0
Release:    1%{?dist}
Summary:    Build tool

Group:      Development/Tools
License:    Apache
URL:        https://github.com/bazelbuild/bazel
Source0:    %{name}-%{version}.zip

BuildRequires: java-1.8.0-openjdk-devel
Requires: java-1.8.0-openjdk-devel

%description
Google build tool

%prep
%setup -c -n %{name}-%{version}

%build
./compile.sh

%install
mkdir -p %{buildroot}/usr/bin
cp ./output/bazel %{buildroot}/usr/bin
cp LICENSE %{_builddir}

%clean
rm -rf ${buildroot}

%files
%defattr(-,root,root)
%license LICENSE
/usr/bin/bazel
