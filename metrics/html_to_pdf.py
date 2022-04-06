#! /usr/bin/env python3

import weasyprint

url='https://gtx201.nrm.minn.seagate.com/~535110/latest/repo_health.slides.html'
out='cache/repo_health.pdf'

pdf = weasyprint.HTML(url).write_pdf(out)
