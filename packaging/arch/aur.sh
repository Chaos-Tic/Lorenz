#!/usr/bin/env bash
# Updates the AUR package to a release: pkgver, checksums and .SRCINFO come
# from packaging/arch/PKGBUILD and makepkg. Runs as root in archlinux:latest
# (the CI release job), from the repository root:
#   AUR_SSH_KEY=... packaging/arch/aur.sh VERSION
set -euo pipefail
version=${1:?usage: packaging/arch/aur.sh VERSION}
pacman -Syu --noconfirm --needed -q git openssh pacman-contrib >/dev/null
install -dm700 ~/.ssh
printf '%s\n' "${AUR_SSH_KEY:?set AUR_SSH_KEY}" >~/.ssh/aur && chmod 600 ~/.ssh/aur
# The AUR host key, as published on https://aur.archlinux.org.
cat >>~/.ssh/known_hosts <<'KEYS'
aur.archlinux.org ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIEuBKrPzbawxA/k2g6NcyV5jmqwJ2s+zpgZGZ7tpLIcN
KEYS
# ssh reads the home folder of the user database, not $HOME (they differ in CI
# containers): name the key and the known hosts file explicitly.
export GIT_SSH_COMMAND="ssh -i $HOME/.ssh/aur -o IdentitiesOnly=yes -o UserKnownHostsFile=$HOME/.ssh/known_hosts"
work=$(mktemp -d)
git clone -q ssh://aur@aur.archlinux.org/lorenz.git "$work/aur"
sed -e "s/^pkgver=.*/pkgver=$version/" -e 's/^pkgrel=.*/pkgrel=1/' packaging/arch/PKGBUILD >"$work/aur/PKGBUILD"
id builder >/dev/null 2>&1 || useradd -m builder
chown -R builder "$work"
(cd "$work/aur" && runuser -u builder -- updpkgsums && runuser -u builder -- makepkg --printsrcinfo >.SRCINFO)
cd "$work/aur"
git add PKGBUILD .SRCINFO
if git diff --cached --quiet; then
    echo "AUR lorenz is already at $version"
else
    git -c user.name=Chaos-Tic -c user.email=58691435+Chaos-Tic@users.noreply.github.com \
        commit -q -m "Update to $version"
    git push -q origin HEAD:master
    echo "AUR lorenz updated to $version"
fi
