%if 0%{?s3_with_python36:1}
%global py_ver 36
%endif
%if 0%{?s3_with_python36_ver8:1}
%global py_ver 3
%endif

%global python_sitearch %python3_sitearch
%global python_sitelib %python3_sitelib
%global __python %__python3
%global py_package_prefix python%{python3_pkgversion}

%{!?python_sitelib: %define python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}

%global commit 4801552f441cf12dec53099a6abc2b8aa36ccca4
%global shortcommit 4801552

Name:           s3cmd
Version:        2.0.2
Release:        1%{dist}
Summary:        Tool for accessing Amazon Simple Storage Service

Group:          Applications/Internet
License:        GPLv2
URL:            http://gerrit.mero.colo.seagate.com:8080/s3cmd
# git clone https://github.com/s3tools/s3cmd
# python setup.py sdist
Source0:        %{name}-%{version}-%{shortcommit}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:      noarch
Patch0:         s3cmd_%{version}_max_retries.patch

BuildRequires:  python36-devel

%if 0%{?s3_with_python36_ver8:1}
BuildRequires:  python3-dateutil
BuildRequires:  python3-setuptools
Requires:       python3-magic
%endif
%if 0%{?s3_with_python36:1}
BuildRequires:  python36-dateutil
BuildRequires:  python36-setuptools
Requires:       python-magic
%endif

%description
S3cmd lets you copy files from/to Amazon S3
(Simple Storage Service) using a simple to use
command line client.


%prep
%setup -q -n %{name}-%{version}-%{shortcommit}
%patch0 -p1

%build


%install
rm -rf $RPM_BUILD_ROOT
S3CMD_PACKAGING=Yes python3.6 setup.py install --prefix=%{_prefix} --root=$RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_mandir}/man1
install -m 644 s3cmd.1 $RPM_BUILD_ROOT%{_mandir}/man1


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_bindir}/s3cmd
%{_mandir}/man1/s3cmd.1*
%{python3_sitelib}/S3
%{python3_sitelib}/s3cmd*.egg-info
%doc NEWS README.md


%changelog
# https://github.com/s3tools/s3cmd/blob/v%{version}/ChangeLog
