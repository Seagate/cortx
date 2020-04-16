# Disable documentation generation for now
%bcond_with docs

Name:           python-scripttest
Version:        1.3.0
Release:        18%{?dist}
Summary:        Helper to test command-line scripts

License:        MIT
URL:            http://pypi.python.org/pypi/ScriptTest/
Source0:        https://github.com/pypa/scripttest/archive/1.3.0.tar.gz

BuildArch:      noarch

BuildRequires: python%{python3_pkgversion}-devel
BuildRequires: python%{python3_pkgversion}-setuptools
%if %{with docs}
BuildRequires: python%{python3_pkgversion}-sphinx
%endif
%if %{with test}
BuildRequires: python%{python3_pkgversion}-pytest
%endif

%description
ScriptTest is a library to help you test your interactive
command-line applications.

With it you can easily run the command (in a subprocess) and see
the output (stdout, stderr) and any file modifications.

%package -n     python%{python3_pkgversion}-scripttest
Summary:        Helper to test command-line scripts
%{?python_provide:%python_provide python%{python3_pkgversion}-scripttest}

%description -n python%{python3_pkgversion}-scripttest
ScriptTest is a library to help you test your interactive
command-line applications.

With it you can easily run the command (in a subprocess) and see
the output (stdout, stderr) and any file modifications.


%prep
%setup -q -n scripttest-%{version}


%build
%py3_build
%if %{with docs}
sphinx-build -b html docs/ docs/html
%endif


%install
%py3_install
%check
%{__python3} setup.py test

%files -n python%{python3_pkgversion}-scripttest
%if %{with docs}
%doc docs/html
%endif
%license docs/license.rst
%{python3_sitelib}/scripttest.py
%{python3_sitelib}/__pycache__/scripttest.cpython-??*
%{python3_sitelib}/scripttest*.egg-info/


%changelog
* Tue Sep 03 2019 Miro Hrončok <mhroncok@redhat.com> - 1.3.0-18
- Remove python2-scripttest

* Sat Aug 17 2019 Miro Hrončok <mhroncok@redhat.com> - 1.3.0-17
- Rebuilt for Python 3.8

* Fri Jul 26 2019 Fedora Release Engineering <releng@fedoraproject.org> - 1.3.0-16
- Rebuilt for https://fedoraproject.org/wiki/Fedora_31_Mass_Rebuild

* Sat Feb 02 2019 Fedora Release Engineering <releng@fedoraproject.org> - 1.3.0-15
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Sat Jul 14 2018 Fedora Release Engineering <releng@fedoraproject.org> - 1.3.0-14
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Mon Jun 18 2018 Miro Hrončok <mhroncok@redhat.com> - 1.3.0-13
- Rebuilt for Python 3.7

* Fri Feb 09 2018 Fedora Release Engineering <releng@fedoraproject.org> - 1.3.0-12
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Tue Jan 30 2018 Iryna Shcherbina <ishcherb@redhat.com> - 1.3.0-11
- Update Python 2 dependency declarations to new packaging standards
  (See https://fedoraproject.org/wiki/FinalizingFedoraSwitchtoPython3)

* Thu Jul 27 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1.3.0-10
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Fri Mar 17 2017 Orion Poplawski <orion@cora.nwra.com> - 1.3.0-9
- Ship python2-scriptest
- Modernize spec

* Sat Feb 11 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1.3.0-8
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Mon Dec 19 2016 Miro Hrončok <mhroncok@redhat.com> - 1.3.0-7
- Rebuild for Python 3.6

* Tue Jul 19 2016 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.3.0-6
- https://fedoraproject.org/wiki/Changes/Automatic_Provides_for_Python_RPM_Packages

* Thu Feb 04 2016 Fedora Release Engineering <releng@fedoraproject.org> - 1.3.0-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Wed Oct 14 2015 Robert Kuska <rkuska@redhat.com> - 1.3.0-4
- Rebuilt for Python3.5 rebuild
- Change pattern for files under __pycache__ folder

* Thu Jun 18 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.3.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Tue Dec 02 2014 Matej Stuchlik <mstuchli@redhat.com> - 1.3.0-2
- Add python3 subpackage

* Wed Jul 02 2014 Matej Stuchlik <mstuchli@redhat.com> - 1.3.0-1
- Update to 1.3.0

* Mon Jun 30 2014 Toshio Kuratomi <toshio@fedoraproject.org> - 1.0.4-10
- Replace python-setuptools-devel BR with python-setuptools

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.0.4-9
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Sun Aug 04 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.0.4-8
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.0.4-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Sat Jul 21 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.0.4-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Sat Jan 14 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.0.4-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.0.4-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Sat Jul 31 2010 Orcan Ogetbil <oget[dot]fedora[at]gmail[dot]com> - 1.0.4-3
- Rebuilt for https://fedoraproject.org/wiki/Features/Python_2.7/MassRebuild

* Wed Jul 21 2010 Martin Bacovsky <mbacovsk@redhat.com> - 1.0.4-2
- generated docs moved to html subdir
- license file added to docs

* Tue Jul 13 2010 Martin Bacovsky <mbacovsk@redhat.com> - 1.0.4-1
- Initial package
- fixed issue preventing build and usage on ext4.

