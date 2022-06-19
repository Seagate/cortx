#! /usr/bin/env python3

import weasyprint


css = weasyprint.CSS(string="""
            @page {
            size: 16in 12in;
            margin: 0in 0.44in 0.2in 0.44in;
        }
    """
      )

url='file:///tmp/Repo_Health.html'
out='cache/repo_health.pdf'

pdf = weasyprint.HTML(url).write_pdf(target=out,stylesheets=[css])
