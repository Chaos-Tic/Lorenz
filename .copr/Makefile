# Fedora COPR builds the source RPM with this (method make_srpm), from the
# repository root, in a chroot with network: the release tarball and the
# vendor archive of the version in Cargo.toml. COPR passes outdir and spec.
version := $(shell sed -n 's/^version = "\(.*\)"/\1/p' Cargo.toml | head -n1)

srpm:
	dnf install -y -q rpmdevtools
	sed 's/^Version: .*/Version:        $(version)/' packaging/fedora/lorenz.spec >$(outdir)/lorenz.spec
	spectool -g -C $(outdir) $(outdir)/lorenz.spec
	rpmbuild -bs $(outdir)/lorenz.spec --define '_sourcedir $(outdir)' --define '_srcrpmdir $(outdir)'
