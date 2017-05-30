# "wpantund" Open-Source Plan #

This document outlines the resources, processes, and procedures for
the launch and ongoing operation of the "wpantund" open-source
project.

## Online Resources ##

 *  Github: <https://github.com/openthread/wpantund/>
     *  Public Wiki: <https://github.com/openthread/wpantund/wiki>
 *  Front-end URL: <http://wpantund.org>
     *  Will initially redirect to
        <https://github.com/openthread/wpantund/#readme>
     *  Content eventually maintained in gh-pages branch.
 *  Mailing Lists
     *  <wpantund-announce@googlegroups.com>
         *  Read-only. Announces releases, security issues, etc.
     *  <wpantund-user@googlegroups.com>
         *  Read-write. For people who are using wpantund.
     *  <wpantund-devel@googlegroups.com>
         *  Read-write. For people who are contributing to wpantund.

## Continuous Integration ##

"wpantund" will use [Travis](https://travis-ci.org/) for continuous
integration testing. All pull requests MUST pass the travis build test
before being considered for merging into the master branch.

## Versoning ##

"wpantund" follows the Semantic Versioning guidelines, with one
exception: the minor and patch versions use a minimum of two digits to
facilitate better sorting.

In short, the version format is of the form `M.mm.pp`, where `M` is
the major version, `mm` is the minor version, and `pp` is the patch
level. Additional suffixes can be appended according to the release
type, as described in the following section.

## Releases ##

A "release" denotes a specific state of the code base that has been
officially designated a unique identifier called the version
(Depending on context, it is sometimes called the release tag). Note
that this section refers to a several git branches which are defined
in later sections.

Broadly, there are three types of releases:

**Primary Releases** — Any release in which the release tag takes the
form of `M.mm.pp`, without any additional annotations. These are the
canonical releases. There are generally two categories of primary
releases:

 *  **Patch Releases** — Increment only the patch version. Made
    periodically as needed.
 *  **Major and Minor Releases** — Patch version is always `00`. Used
    as feature milestones and are planned well in advance.

**Pre-Releases** — Any release leading up to a primary release. There
are three different types of pre-releases:

 *  **Alpha Releases** — which have the suffix `a#`, e.g.: `1.02.03a4`
 *  **Beta Releases** — which have the suffix `b#`, e.g.: `1.02.03b4`
 *  **Release Candidates** — which have the suffix `rc#`, e.g.:
    `1.02.03rc4`

**Special Releases** — Any release that is intended for a special
purpose or task. Denoted by the suffix `-*`, e.g. `1.02.03-special`
or `4.56.78a9-foobar`.

### Release Process Output ###

"Cutting a release" involves the creation and updating of tarballs,
branches, and tags. The following git tags are created or updated:

 *  `<VERSION>` — The identifying tag of the release, pointing to
    the commit that is being released. The contents of this tag
    contain the information contained in the CHANGELOG which has been
    added since the previous release this release was based on. This
    tag is cryptographically signed with the OpenPGP signature of a
    project maintainer.  Ex: `1.02.03a4`
 *  `full/<VERSION>` - A merge commit between the release tag and the
    corresponding commit from `autoconf/master` (See the git
    repository plan later in this document). In other words, this tag
    points to the actual files that are released in the official
    tarballs. Created for each of the two above tags (contains
    autoconf files). This tag is cryptographically signed with the
    OpenPGP signature of a project maintainer.  Ex: `full/1.02.03a4`
 *  `latest-unstable` — Updated to point to the release tag ONLY if
    the version of this release is larger than the previous version
    this tag points to. This is a pointer-only tag.
 *  `latest-release` — Updated to point to the release tag ONLY if
    this release is a primary release and ONLY if version of this
    release is larger than the previous version this tag points to.
    This is a pointer-only tag, and MUST NOT contain a message.
 *  `full/latest-unstable` / `full/latest-release` — Created for
    each of the two above tags (contains autoconf files).

Tarballs are created by checking out the `full/<VERSION>` tag (created
above) and performing a `./configure && make distcheck`. The resulting
files are the official tarballs. All released tarballs are
cryptographically signed with the OpenPGP signature of a maintainer.

### Release Checklist ###

The following is the checklist followed by a project maintainer to cut
an official release from the top of the master branch:

1.  Clone a clean copy of the repository from the official repository
    on GitHub. If this fails, STOP.
2.  Do a `git checkout master`. If this fails, STOP.
3.  Do a `git show`. Verify that Travis has passed the given commit.
    If this commit has not passed, STOP.
4.  Collect the changes that have been included since the last release
    and update the contents of the CHANGELOG accordingly.
5.  Verify that the project version in `.default-version` is set to
    the version that is about to be released. This should already be
    the case, but if it isn’t go ahead and update `.default-version`
     to the version of the release.
6.  Commit the changes made to the working directory from steps 2 and
    3\. Use a commit message like `Updated CHANGELOG for release
    <VERSION>` or `Updated CHANGELOG and configure.ac for release
    <VERSION>`.
7.  If this is a prerelease, copy the section that you just added to
    the CHANGELOG to the clipboard. If this is a primary release,
    copy the contents of the CHANGELOG added after the last primary
    release to the clipboard.
8.  Create a signed tag for the release named `<VERSION>`, making sure
    pasting in the contents we copied to the clipboard as the tag
    message. This is performed with the command `git tag -s
    <VERSION>`. If this command fails, STOP.
9.  Check out the `origin/scripts` branch: `git checkout
    origin/scripts`. If this command fails, STOP.
10. If this is a primary release execute the command
    `./primary-release-from-master-helper`; or if this is a
    prerelease execute the command
    `./pre-release-from-master-helper`. These commands update
    `autoconf/master`, `full/<VERSION>`, `latest-unstable`,
    `full/latest-unstable` and (if this is a primary release)
    `latest-release`, `full/latest-release`. If the command fails,
    STOP. Note that the tag `full/<VERSION>` will be signed by this
    process, so you may be asked for your GPG keychain credentials or
    your token PIN.
11. Check out the `full/<VERSION>` branch: `git checkout
    full/<VERSION>`. If this command fails, STOP.
12. Execute the command `./configure && make distcheck`. If this
    command fails, STOP.
13. There should now be at least one tarball (maybe more) in the
    current directory. If this is not the case, STOP.
14. Use GPG to sign the tarballs. If this process fails, STOP.
15. Type `git show master`. Review the commit to make sure it looks
    sane. If you notice anything wrong, STOP.
16. Type `git show <VERSION>`. Review the tag notes make they look
    properly formatted. If you notice anything wrong, STOP.
17. Push the branches and tags to the origin server: `git push origin
    master autoconf/master <VERSION> full/<VERSION>`. If this command
    fails, STOP.
18. Update the `latest-*` tags on the origin server: `git push origin
    latest-release latest-unstable full/latest-release
    full/latest-unstable`
19. Upload the tarballs and their signatures to the designated
    location. (Currently TBD)
20. ???
21. Pr0fit!!1

Note that the above checklist is only appropriate for cutting releases
from the master branch! Cutting releases from non-master branches
would follow a similar (but different) process that is not yet
defined.

## Public Git Repository Branch and Tag Plan ##

### Branches ###

 *  `master` — Main development branch. Acceptable commits are
    generally limited to those intended for the next major/minor
    release. Does not contain any autoconf-generated files!
 *  `autoconf/master` — All of (and only!) the autoconf-generated
    files associated with master. Automatically maintained by a script
    that checks if a commit to master has invalidated any of these
    files and commits updates.
 *  `scripts` — A collection of repository maintenance scripts.
 *  `gh-pages` — Will eventually contain the content of
    <http://wpantund.org/>.
 *  `feature/*` — Feature-specific branches that are currently
    under development but not yet appropriate for inclusion in master.
 *  `release/M.mm/master` — A separate branch maintained for each
    minor version release, for tracking things like security updates
    and back-ported changes. These branches are created when
    master is opened to taking commits for the next major/minor
    release.

### Tags ###

 *  `M.mm.pp` — Each primary release is tagged with the bare
    version number of the release.
 *  `M.mm.ppx#` — Each pre-release is tagged with the version of
    the upcoming release with a suffix `x` denoting the type of
    pre-release and `#` denoting the count.
 *  `M.mm.pp-x` — Each special-purpose release is tagged with the
    version of the most recent official release (for any other type),
    suffixed with a dash and a short descriptor of the release.
 *  `full/*` — For each release (primary, pre, or
    special-purpose), the tag that is the name of the release prefixed
    with `full/` points to a merge commit between the release tag and
    the corresponding commit from `autoconf/master`. In other words,
    this tag points to the actual files that are released in the
    official tarballs.
 *  `latest-release` — A tag with no message that points to the tag
    of the most recent primary release.
 *  `full/latest-release` — A tag with no message that points to
    the tag of the most recent primary release in `full/*`.
 *  `latest-unstable` — A tag with no message that points to the
    tag of the most recent primary release or pre-release.
 *  `full/latest-unstable` — A tag with no message that points to
    the tag of the most recent primary release or pre-release in
    `full/*`.

### Regarding Autotools ###

No generated autotools files are allowed in master, or in any branch
that doesn’t begin with `autoconf/` or `full/`.

If you are working from the top of master and don’t have a working
copy of autotools, the following command will populate your working
tree with all of the appropriate files and is the functional
equivalent of typing `./bootstrap.sh` on a machine with autotools
installed:

	git archive origin/autoconf/master | tar xv

Note that the `full/*` branches contain the full set of files,
including the sources and autotools files. The `autoconf/*` branches
contain ONLY the autoconf files.
