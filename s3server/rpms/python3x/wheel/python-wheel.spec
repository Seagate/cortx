%global pypi_name wheel

Name:           python-%{pypi_name}
Version:        0.24.0
Release:        2%{?dist}
Summary:        A built-package format for Python

License:        MIT
URL:            http://bitbucket.org/dholth/wheel/
Source0:        https://pypi.python.org/packages/source/w/%{pypi_name}/%{pypi_name}-%{version}.tar.gz
# Some test files are not present in tarball, so we include them separately.
# Upstream pull request to include the files in tarball:
# https://bitbucket.org/dholth/wheel/pull-request/34 (Patch0 below)
# (version 0.22 doesn't have a tag, so we're using commit hash to point to the
#  correct testing wheel)
Source1:        https://bitbucket.org/dholth/wheel/raw/099352e/wheel/test/test-1.0-py2.py3-none-win32.whl
Source2:        https://bitbucket.org/dholth/wheel/raw/099352e/wheel/test/pydist-schema.json
BuildArch:      noarch

BuildRequires:  python-devel
BuildRequires:  python-setuptools

BuildRequires:  pytest
BuildRequires:  python-jsonschema
BuildRequires:  python-keyring

%if 0%{?s3_with_python34:1}
BuildRequires:  python%{python3_pkgversion}-devel
BuildRequires:  python%{python3_pkgversion}-setuptools
%endif # if with_python34

%if 0%{?s3_with_python36:1}
BuildRequires:  python%{python3_other_pkgversion}-devel
BuildRequires:  python%{python3_other_pkgversion}-setuptools
%endif # if with_python36


%description
A built-package format for Python.

A wheel is a ZIP-format archive with a specially formatted filename and the
.whl extension. It is designed to contain all the files for a PEP 376
compatible install in a way that is very close to the on-disk format.

%if 0%{?s3_with_python34:1}
%package -n     python%{python3_pkgversion}-%{pypi_name}
Summary:        A built-package format for Python

%description -n python%{python3_pkgversion}-%{pypi_name}
A built-package format for Python.

A wheel is a ZIP-format archive with a specially formatted filename and the
.whl extension. It is designed to contain all the files for a PEP 376
compatible install in a way that is very close to the on-disk format.

This is package contains Python 3 version of the package.

%endif # with_python3

%if 0%{?s3_with_python36:1}
%package -n     python%{python3_other_pkgversion}-%{pypi_name}
Summary:        A built-package format for Python

%description -n python%{python3_other_pkgversion}-%{pypi_name}
A built-package format for Python.

A wheel is a ZIP-format archive with a specially formatted filename and the
.whl extension. It is designed to contain all the files for a PEP 376
compatible install in a way that is very close to the on-disk format.

This is package contains Python 3 version of the package.
%endif # with_python36



%prep
%setup -q -n %{pypi_name}-%{version}

# copy test files in place
cp %{SOURCE1} %{pypi_name}/test/
cp %{SOURCE2} %{pypi_name}/test/
# header files just has to be there, even empty
touch %{pypi_name}/test/headers.dist/header.h

# remove unneeded shebangs
sed -ie '1d' %{pypi_name}/{egg2wheel,wininst2wheel}.py

%if 0%{?s3_with_python34:1}
rm -rf %{py3dir}
cp -a . %{py3dir}
%endif # with_python3

%if 0%{?s3_with_python36:1}
rm -rf %{py3dir}-for%{python3_other_pkgversion}
cp -a . %{py3dir}-for%{python3_other_pkgversion}
%endif # with_python36


%build
%{__python} setup.py build

%if 0%{?s3_with_python34:1}
pushd %{py3dir}
%{__python3} setup.py build
popd
%endif # with_python3

%if 0%{?s3_with_python36:1}
pushd %{py3dir}-for%{python3_other_pkgversion}
%{__python3_other} setup.py build
popd
%endif # with_python36


%install
# Must do the subpackages' install first because the scripts in /usr/bin are
# overwritten with every setup.py install (and we want the python2 version
# to be the default for now).
%if 0%{?s3_with_python34:1}
pushd %{py3dir}
%{__python3} setup.py install --skip-build --root %{buildroot}
popd
pushd %{buildroot}%{_bindir}
for f in $(ls); do mv $f python%{python3_pkgversion}-$f; done
popd
%endif # with_python3

%if 0%{?s3_with_python36:1}
pushd %{py3dir}-for%{python3_other_pkgversion}
%{__python3_other} setup.py install --skip-build --root %{buildroot}
popd
pushd %{buildroot}%{_bindir}
for f in $(ls -1 | grep -v '^python'); do mv $f python%{python3_other_pkgversion}-$f; done
popd
%endif # with_python36

%{__python} setup.py install --skip-build --root %{buildroot}


%check
# remove setup.cfg that makes pytest require pytest-cov (unnecessary dep)
rm setup.cfg
PYTHONPATH=$(pwd) py.test --ignore build
# no test for Python 3, no python3-jsonschema yet
%if 0
pushd %{py3dir}
rm setup.cfg
PYTHONPATH=$(pwd) py.test-%{python%{python3_pkgversion}_version} --ignore build
popd
%endif # with_python3

# not tested for python_other, just as for python3 above
%if 0
pushd %{py3dir}-for%{python3_other_pkgversion}
rm setup.cfg
PYTHONPATH=$(pwd) py.test-%{python%{python3_other_pkgversion}_version} --ignore build
popd
%endif # have python3_other


%files
%doc LICENSE.txt CHANGES.txt README.txt
%{_bindir}/wheel
%{python_sitelib}/%{pypi_name}*
%exclude %{python_sitelib}/%{pypi_name}/test

%if 0%{?s3_with_python34:1}
%files -n python%{python3_pkgversion}-%{pypi_name}
%doc LICENSE.txt CHANGES.txt README.txt
%{_bindir}/python%{python3_pkgversion}-wheel
%{python3_sitelib}/%{pypi_name}*
%exclude %{python3_sitelib}/%{pypi_name}/test
%endif # with_python3

%if 0%{?s3_with_python36:1}
%files -n python%{python3_other_pkgversion}-%{pypi_name}
%doc LICENSE.txt CHANGES.txt README.txt
%{_bindir}/python%{python3_other_pkgversion}-wheel
%{python3_other_sitelib}/%{pypi_name}*
%exclude %{python3_other_sitelib}/%{pypi_name}/test
%endif # with_python36


%changelog
* Sat Jan 03 2015 Matej Cepl <mcepl@redhat.com> - 0.24.0-2
- Make python3 conditional (switched off for RHEL-7; fixes #1131111).

* Mon Nov 10 2014 Slavek Kabrda <bkabrda@redhat.com> - 0.24.0-1
- Update to 0.24.0
- Remove patches merged upstream

* Sun Jun 08 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.22.0-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Fri Apr 25 2014 Matej Stuchlik <mstuchli@redhat.com> - 0.22.0-3
- Another rebuild with python 3.4

* Fri Apr 18 2014 Matej Stuchlik <mstuchli@redhat.com> - 0.22.0-2
- Rebuild with python 3.4

* Thu Nov 28 2013 Bohuslav Kabrda <bkabrda@redhat.com> - 0.22.0-1
- Initial package.
