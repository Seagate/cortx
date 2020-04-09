Contributing to Mero
====================

## Prerequisites

The web interface and repositories for Mero are hosted inside
Seagate's private network. If you are working from outside a Seagate
site, you will need to set up a VPN connection. Refer to
[Seagate's guide](https://sites.google.com/a/seagate.com/it-remote-working-centre/vpn).

## Set up your local repository

### Add your public key to Gerrit

You will need Gerrit to know your public key in order to connect to
the server via SSH. If you haven't already done so, follow these steps
to register your public key:

-   Log in to [Gerrit's web interface](http://es-gerrit.xyus.xyratex.com:8080/).
-   Go to the [SSH Public Keys settings](http://es-gerrit.xyus.xyratex.com:8080/#/settings/ssh-keys).
-   Click the "Add Key" button.
-   Copy the content of your public key file (*e.g.* `id_rsa.pub`) in
    the text box and press "Add" then "Close".

### Clone the repository

Clone the repository with

    git clone --recursive ssh://USERNAME@es-gerrit.xyus.xyratex.com:29418/mero

Where `USERNAME` must be replaced by to your username on Gerrit.

> Go to your
> [Gerrit profile](http://es-gerrit.xyus.xyratex.com:8080/#/settings/)
> to find your Gerrit username. Your username is typically 'g'
> followed by your GID (for example, if your GID is '123456', your
> Gerrit username would be 'g123456').

IMPORTANT: To be able to push changes for review, you need to install a commit
hook by running the following command in the Mero directory:

    $ scp -p -P 29418 USERNAME@es-gerrit.xyus.xyratex.com:hooks/commit-msg .git/hooks/

Where `USERNAME` must be replaced by your username on Gerrit.

Make sure that your email address for this repository is configured to
be an address that Gerrit knows:

    $ git config user.email NAME@seagate.com

## Prepare your contribution

Before sending your patches for review, rebase them on top of
`origin/master`. Then, check them at least with:

    $ ./scripts/m0 run-ut

Ideally, run the complete tests with:

    $ ./scripts/m0 check-everything

It is better, for the review process, that your contribution contains
no more than 3 commits.

## Submit your contribution

Push your changes for review with

    $ git push origin HEAD:refs/for/master

This pushes the current commits to a virtual reference which will
create a review request for you and start continuous integration tests
automatically. You will see a message of the following form:

    remote: New Changes:
    remote:   http://es-gerrit.xyus.xyratex.com:8080/<review-number>

Follow the link. There, add an reviewer for your review request by
clicking the "Add Reviewer" button and selecting the reviewer's name
or email address.

### Note on development branch

You can push development branches to the Gerrit repository. They must
be named `dev/<branch-name>`.

Gerrit refuses to push existing commit for review, so if you want to
submit commits from your development branch, you need to change their
names. This can be done with the following one-liner:

    $ git rebase -f `git merge-base HEAD origin/master`

## Address reviewer comments

If you need to address comments from the reviewer, commit your
changes then rebase your patches on top of master. Finally submit your
patches with:

    $ git push origin HEAD:refs/for/master

Gerrit will automatically update your review request.

## Land you contribution

Once tests pass and the reviewer gives you their approval, go the
[#mero-gatekeepers channel on Slack](https://seagate.slack.com/messages/mero-gatekeepers/)
and ask the gatekeepers to add your patches to the landing queue.

## Further reading

If you are experiencing troubles or if you need tips for advanced
workflows please consult
[the Mero-Gerrit FAQ](https://docs.google.com/document/d/1cRqKhEWLQt23QuwfmvVE5DWDpGBbFlYI8FmGCezVa2o/).
