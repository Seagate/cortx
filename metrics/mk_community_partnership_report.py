#! /usr/bin/env python3 

from atlassian import Confluence

import os
import PyPDF2


token=os.getenv('JI_OATH')

conf=Confluence(url='https://seagate-systems.atlassian.net',
                api_version='cloud',
                username='john.bent@seagate.com',
                password=token)

def get_confluence_pid(title):
    space='PUB'
    pid=conf.get_page_id(space,title)
    return pid

def get_pdf(title,pid=None):
    space='PUB'
    if pid is None:
      pid=conf.get_page_id(space,title)
    content=conf.export_page(pid)
    print("Fetching %s (%s)" % (title,pid))
    pdf='./cache/%s.pdf' % title
    with open(pdf,'wb') as pdf_file:
        pdf_file.write(content)
    pdf_in = open(pdf, 'rb')
    pdf_reader = PyPDF2.PdfFileReader(pdf_in)
    page = pdf_reader.getPage(0)
    #pdf_in.close()
    return page
    #page.rotateCounterClockwise(90) # also rotates contents, don't want that
    #pdf_in.close()
    #return pdf

def main():

  pdf_writer = PyPDF2.PdfFileWriter()
  for page in ('CORTX Community Projects Overview', 'CORTX Community Project Roadmap'):
    page = get_pdf(page)
    pdf_writer.addPage(page)
  page = get_pdf('CORTX Community Partnerships Overview')
  pdf_writer.addPage(page)
  page = get_pdf('CORTX Community Project Roadmap')
  pdf_writer.addPage(page)

  parent=get_confluence_pid('Projects')
  children=conf.get_page_child_by_type(parent, type='page', start=None, limit=None)
  for child in sorted(children, key = lambda i: i['title']):
    page = get_pdf(child['title'],child['id'])
    pdf_writer.addPage(page)

  pdf_out = open('CORTX_Community_Partnerships_Review.pdf', 'wb')
  pdf_writer.write(pdf_out)
  pdf_out.close()


if __name__ == "__main__":
    main()
