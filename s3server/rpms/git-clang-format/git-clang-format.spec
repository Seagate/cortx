Name:		git-clang-format
Version:	6.0
Release:	1%{?dist}
Summary:	clang-format integration for git

License:	NCSA
URL:		http://llvm.org
Source0:	%{name}-%{version}.tar.gz
Requires:	clang >= 3.4
Requires:	wget
Requires:	git


%description -n git-clang-format
clang-format integration for git.

%prep
%setup -q

%install
rm -rf %{buildroot}

install -d $RPM_BUILD_ROOT%{_bindir}/

cp git-clang-format $RPM_BUILD_ROOT%{_bindir}/
cp LICENSE.TXT %{_builddir}

%clean
rm -rf %{buildroot}

%files -n git-clang-format
%attr(755, root, root) /usr/bin/git-clang-format
%license LICENSE.TXT
%{_bindir}/git-clang-format
