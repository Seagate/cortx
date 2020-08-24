# Contributing by Converting Binary Documentation

Thanks for your interest in joining our community.  One of the things that we have realized as we are open sourcing CORTX is that
we have produced a lot of documents that tend to drift away from the code.  The major reason for this is that the documents are produced
in some binary format (e.g. MS Word or MS Powerpoint) and stored in a different location from the code.  Clearly it would be better for
more of our documentation to live alongside the code in the same repository.  This will ensure that they are not separated in the future
and should provide great documentation for new community members to quickly learn about CORTX as well as opportunities for easy ways for
new members to begin contributing to the community perhaps before they feel confident about embarking on a large code contribution.

As such, we are providing a list of binary documents stored elsewhere that we believe would be better suited as text documents stored
alongside our code within one of our 'doc/' folders.  For the conversion, we are requesting that the final format of the documents be
either [reStructuredText](https://docutils.sourceforge.io/rst.html) or [MarkDown](https://www.markdownguide.org/) as these formats are
very simple to write, are utilitarian yet elegant to view, and can be browsed directly through our GitHub repositories.  In general, please use reStructuredText for most documents and only use MarkDown for very small and very simple documents.

Please find the list [here](https://seagatetechnology.sharepoint.com/:x:/s/cortx-innersource/EWhpumcTSsBNj5khvGPigU8BXQuzlWEutAvxa80u2bNrGw?e=g9uhQA) and add your name in the 'Assignee' column to ensure that multiple community members do not inadvertently, unnecessarily perform the same activities.  Please also add the date on which you assigned it to yourself in the 'Date Assigned' column.

Once you have completed the conversion, please also edit an existing documentation file to add a pointer to your new file and then submit a patch (pull request) containing the new file and the modified file.  Thanks!

Motr High-level designs
=======================

When working on converting motr high-level designs (HLDs) into rst, please use [doc/hld/hld-template.rst.in](https://github.com/Seagate/cortx-motr/blob/dev/architecture/doc/hld/hld-template.rst.in) in
[dev/architecture branch](https://github.com/Seagate/cortx-motr/tree/dev/architecture) as the template.

Note, that rst files in dev/architecture are processed by m4 macros during the build process (look for rst.m4 files
in this branch). This is a generally useful practice to make documents more uniform, let's follow it.
