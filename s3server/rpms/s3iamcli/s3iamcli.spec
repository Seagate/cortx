# build number
%define build_num  %( test -n "$build_number" && echo "$build_number" || echo 1 )

%global py_ver 3.6

%if 0%{?el7}
# pybasever without the dot:
%global py_short_ver 36
%endif

%if 0%{?el8}
# pybasever without the dot:
%global py_short_ver 3
%endif

# XXX For strange reason setup.py uses /usr/lib
# but %{_libdir} resolves to /usr/lib64 with python3.6
#%global py36_sitelib %{_libdir}/python%{py_ver}
%global py36_sitelib /usr/lib/python%{py_ver}/site-packages

Name:       eos-s3iamcli
Version:    %{_s3iamcli_version}
Release:    %{build_num}_%{_s3iamcli_git_ver}
Summary:    Seagate S3 IAM CLI.

Group:      Development/Tools
License:    Seagate
URL:        http://gerrit.mero.colo.seagate.com:8080/s3server
Source0:    %{name}-%{version}-%{_s3iamcli_git_ver}.tar.gz

BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix:     %{_prefix}
BuildArch:  noarch
Vendor:     Seagate

BuildRequires:  python3-rpm-macros
BuildRequires:  python36
BuildRequires:  python%{py_short_ver}-devel
BuildRequires:  python%{py_short_ver}-setuptools
BuildRequires:  python%{py_short_ver}-wheel

Requires:  python36
Requires:  python%{py_short_ver}-yaml
Requires:  python%{py_short_ver}-xmltodict >= 0.9.0
Requires:  python%{py_short_ver}-jmespath >= 0.7.1
Requires:  python%{py_short_ver}-botocore >= 1.5.0
Requires:  python%{py_short_ver}-s3transfer >= 0.1.10
Requires:  python%{py_short_ver}-boto3 >= 1.4.6

%description
Seagate S3 IAM CLI

%package        devel
Summary:        Development files for %{name}
Group:          Development/Tools

BuildRequires:  python3-rpm-macros
BuildRequires:  python36
BuildRequires:  python%{py_short_ver}-devel
BuildRequires:  python%{py_short_ver}-setuptools

Requires:  python36
Requires:  python%{py_short_ver}-yaml
Requires:  python%{py_short_ver}-xmltodict >= 0.9.0
Requires:  python%{py_short_ver}-jmespath >= 0.7.1
Requires:  python%{py_short_ver}-botocore >= 1.5.0
Requires:  python%{py_short_ver}-s3transfer >= 0.1.10
Requires:  python%{py_short_ver}-boto3 >= 1.4.6

%description    devel
This package contains development files for %{name}.


%prep
%setup -n %{name}-%{version}-%{_s3iamcli_git_ver}

%build
mkdir -p %{_builddir}/%{name}-%{version}-%{_s3iamcli_git_ver}/build/lib/s3iamcli
cd s3iamcli
python%{py_ver} -m compileall -b *.py
cp  *.pyc %{_builddir}/%{name}-%{version}-%{_s3iamcli_git_ver}/build/lib/s3iamcli
echo "build complete"

%install
cd %{_builddir}/%{name}-%{version}-%{_s3iamcli_git_ver}
python%{py_ver} setup.py install --no-compile --single-version-externally-managed -O1 --root=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_bindir}/s3iamcli
%{py36_sitelib}/s3iamcli/config/*.yaml
%{py36_sitelib}/s3iamcli-%{version}-py?.?.egg-info
%{py36_sitelib}/s3iamcli/*.pyc
%exclude %{py36_sitelib}/s3iamcli/__pycache__/*
%exclude %{py36_sitelib}/s3iamcli/*.py
%exclude %{py36_sitelib}/s3iamcli/s3iamcli
%defattr(-,root,root)

%files devel
%{_bindir}/s3iamcli
%{py36_sitelib}/s3iamcli/*.py
%{py36_sitelib}/s3iamcli/config/*.yaml
%{py36_sitelib}/s3iamcli-%{version}-py?.?.egg-info
%exclude %{py36_sitelib}/s3iamcli/*.pyc
%exclude %{py36_sitelib}/s3iamcli/__pycache__/*
%exclude %{py36_sitelib}/s3iamcli/s3iamcli
%defattr(-,root,root)
