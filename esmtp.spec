Summary: 	User configurable relay-only Mail Transfer Agent (MTA)
Name:		esmtp
Version:	0.4.1
Release:	1
Source: 	http://belnet.dl.sourceforge.net/sourceforge/%{name}/%{name}-%{version}.tar.bz2
Url:		http://esmtp.sourceforge.net/
License:	GPL
Group:		Networking/Mail
BuildRoot:	%{_tmppath}/%{name}-root
Requires:	libesmtp
BuildRequires:	libesmtp-devel
Packager:	Robert Scheck <esmtp@robert-scheck.de>
Vendor:		Robert Scheck <esmtp@robert-scheck.de>

%description
ESMTP is a user configurable relay-only Mail Transfer Agent (MTA) with a
sendmail-compatible syntax. It's based on libESMTP supporting the AUTH
(including the CRAM-MD5 and NTLM SASL mechanisms) and the StartTLS SMTP
extensions.

So these are ESMTP features:
 * requires no administration privileges
 * individual user configuration
 * sendmail command line compatible
 * supports the AUTH SMTP extension, with the CRAM-MD5 and NTLM SASL
   mechanisms
 * support the StartTLS SMTP extension
 * does not receive mail, expand aliases or manage a queue

%prep
%setup -q

%build
%configure
make

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc AUTHORS README TODO ChangeLog sample.esmtprc
%{_bindir}/esmtp
%{_mandir}/man1/esmtp.1.gz
%{_mandir}/man5/esmtprc.5.gz

%changelog
* Mon Oct 27 2003 Robert Scheck <esmtp@robert-scheck.de> 0.4.1-1
- Update to 0.4.1
- Initial Release for Red Hat Linux
