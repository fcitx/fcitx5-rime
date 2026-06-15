# Fedora/openSUSE RPM spec for fcitx5 scheme addons
#
# Usage:
#   rpmbuild -ba fcitx5-scheme.spec \
#     --define "scheme_id hanlo" \
#     --define "scheme_name 意傳教育部漢羅" \
#     --define "scheme_submodule Rime-HanLo" \
#     --define "scheme_icon_too kip-hanlo" \
#     --define "scheme_label 漢" \
#     --define "scheme_lang_code nan-TW"

%if 0%{!?scheme_id:1}
%{error:scheme_id must be defined}
%endif
%if 0%{!?scheme_name:1}
%{error:scheme_name must be defined}
%endif
%if 0%{!?scheme_submodule:1}
%{error:scheme_submodule must be defined}
%endif
%if 0%{!?scheme_icon_too:1}
%{error:scheme_icon_too must be defined}
%endif

%global scheme_label    %{?scheme_label}%{!?scheme_label:%{scheme_id}}
%global scheme_lang_code %{?scheme_lang_code}%{!?scheme_lang_code:nan-TW}

Name:           fcitx5-%{scheme_id}
Version:        1.0.0
Release:        1%{?dist}
Summary:        %{scheme_name} Input Method for Fcitx5
License:        LGPL-2.1-or-later
URL:            https://ithuan.tw
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.6
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig
BuildRequires:  cmake(Fcitx5Core)
BuildRequires:  cmake(Fcitx5Module)
BuildRequires:  pkgconfig(rime) >= 1.7.0

Requires:       fcitx5
Requires:       librime >= 1.7.0

%description
%{scheme_name} Taiwanese input method for Fcitx5,
powered by the Rime engine.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake \
    -DSCHEME_ID=%{scheme_id} \
    -DSCHEME_NAME="%{scheme_name}" \
    -DSCHEME_SUBMODULE=%{scheme_submodule} \
    -DSCHEME_LABEL="%{scheme_label}" \
    -DSCHEME_LANG_CODE=%{scheme_lang_code} \
    -DSCHEME_ICON_TOO=%{scheme_icon_too}
%cmake_build

%install
%cmake_install

%files
%license LICENSES/*
%{_libdir}/fcitx5/lib%{scheme_id}.so
%{_datadir}/fcitx5/addon/%{scheme_id}.conf
%{_datadir}/fcitx5/inputmethod/%{scheme_id}.conf
%{_datadir}/%{scheme_id}/data/
%{_datadir}/icons/hicolor/*/apps/fcitx-%{scheme_id}.*
%{_datadir}/icons/hicolor/*/apps/fcitx_%{scheme_id}_*

%changelog
