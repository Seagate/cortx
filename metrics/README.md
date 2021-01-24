### CORTX Community Scripts

This folder contains a bunch of scripts used by track CORTX Community Metrics.  They are intended to be run using crontab and piped into mail.
They also persist data into pickles and json files as found in the [pickles/ child folder](pickles).  Those pickles should also be automatically updated
using a crontab job.

The main scripts in this folder are the following:
1. **[scrape_metrics.py](scrape_metrics.py)**
    * This is the main worker program that crawls github and puts a ton of info into pickles       
    * It creates two main pickles: one for actions and one for people
    
2. **[get_personal_activity.py](get_personal_activity.py)**
    * This program will print all known activity by one or community members
    
3. **[cortx_people.py](cortx_people.py)**
    * This program allows making modifications to the people pickle    
    * Use this for example to mark which community members are external or not    
    * Run this with the _dump_ flag to see all community members
    
4. **[print_metrics.py](print_metrics.py)**
    * This program reads the pickles and prints the data from the last time the scrape was done

5. **[cortx_community.py](cortx_community.py)**
    * This is the module imported by the other python scripts which handles the pickling and various other utility functions
    
6. **[cortx_graphing.py](cortx_graphing.py)**
    * This is the module imported by two of the Jupyter notebooks which has useful functions for making graphs.
    
7. **[weekly_report.sh](weekly_report.sh)**
    * A trivial bash script that calls the above and pipes output into mail; intended to be run as a cron job    
    * E.g. 30 11 * * sat bash -c '/home/535110/cortx/metrics/weekly_report.sh' -> run it every Saturday at 11:30 AM
    * It also produces and mails two reports produced by two of the Jupyter notebooks.
    
8. **[commit_pickles.sh](commit_pickles.sh)**
    * A trivial bash script that pushes the pickles into the repo.  Run as part of the above weekly_report.sh

9. **[quarterly_innersource.sh](quarterly_innersource.sh)**
    * A trivial bash script that produces the quarterly report for evaluating Seagate innersource activity
   
10. A few Jupyter notebooks
    * **[CORTX_Metrics_Explore.ipynb](CORTX_Metrics_Explore.ipynb)** : useful for interactive exploration and can be used to manually populate the small number of metrics which are not yet automatically scraped
    * **[CORTX_Metrics_Topline_Report.ipynb](CORTX_Metrics_Topline_Report.ipynb)** : produces the executive report containing the subset of metrics that we think best summarize and incentivize our community goals
    * **[CORTX_Metrics_Graphs.ipynb](CORTX_Metrics_Graphs.ipynb)** : produces a few other specific interesting graphs produced with some derived values as well as producing a bulk set of graphs for every metric
    

    
For more info, pass '-h' or '--help' to any of the python scripts.  Some of the main python modules which you will need to install include: requests, xlrd, python-dateutil, and pyGithub.

To display the Executive Report as slides, run:
jupyter nbconvert --to slides --post serve --SlidesExporter.reveal_theme=serif --SlidesExporter.reveal_scroll=True CORTX_Metrics_Topline_Report.ipynb
Then view it at http://127.0.0.1:8000/CORTX_Metrics_Topline_Report.slides.html.
To print it to PDF, view http://127.0.0.1:8000/CORTX_Metrics_Topline_Report.slides.html?print-pdf#/ and then print to PDF.  Make sure margins are none and it is in landscape mode.
