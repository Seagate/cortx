#! /usr/bin/env python3

import pandas as pd
import cortx_community
import matplotlib.pyplot as plt


# a helper function to get a dataframe
def get_dataframe(repo,ps):
  (latest,date)=ps.get_latest(repo)
  data={}
  strdates=ps.get_dates(repo)
  dates = []
  for date in strdates:
    dates.append(pd.to_datetime(date))
  # load everything into a dataframe
  for k in latest.keys():
    data[k]=ps.get_values_as_numbers(repo,k)
  df=pd.DataFrame(data=data,index=dates)
  try:
    df=df.drop('2020-12-20',axis=0) # this first scrape was no good, double counted issues and pulls
  except KeyError:
    pass # was already dropped
  return df

class Goal:
  def __init__(self, name, date, value):
    self.name = name
    self.date = date
    self.value = value


def goal_graph(df,title,xlim,ylim,goals,columns):
  df[columns].plot(title=title,xlim=xlim,ylim=ylim)
  plt.legend(loc='lower right')
  for goal in goals:
    plt.annotate(goal.name, (goal.date, goal.value))
  return plt
