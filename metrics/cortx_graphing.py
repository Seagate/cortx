#! /usr/bin/env python3

import pandas as pd
import cortx_community
import matplotlib.pyplot as plt

# manually copied in from seaborn
default_colors=[(0.12156862745098039, 0.4666666666666667, 0.7058823529411765), (1.0, 0.4980392156862745, 0.054901960784313725), (0.17254901960784313, 0.6274509803921569, 0.17254901960784313), (0.8392156862745098, 0.15294117647058825, 0.1568627450980392), (0.5803921568627451, 0.403921568627451, 0.7411764705882353), (0.5490196078431373, 0.33725490196078434, 0.29411764705882354), (0.8901960784313725, 0.4666666666666667, 0.7607843137254902), (0.4980392156862745, 0.4980392156862745, 0.4980392156862745), (0.7372549019607844, 0.7411764705882353, 0.13333333333333333), (0.09019607843137255, 0.7450980392156863, 0.8117647058823529)]
pastels=[(0.6313725490196078, 0.788235294117647, 0.9568627450980393), (1.0, 0.7058823529411765, 0.5098039215686274), (0.5529411764705883, 0.8980392156862745, 0.6313725490196078), (1.0, 0.6235294117647059, 0.6078431372549019), (0.8156862745098039, 0.7333333333333333, 1.0), (0.8705882352941177, 0.7333333333333333, 0.6078431372549019), (0.9803921568627451, 0.6901960784313725, 0.8941176470588236), (0.8117647058823529, 0.8117647058823529, 0.8117647058823529), (1.0, 0.996078431372549, 0.6392156862745098), (0.7254901960784313, 0.9490196078431372, 0.9411764705882353)]
blacks=['black','red','grey']

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
    dropdate=pd.to_datetime('2020-12-20')
    df=df.drop(dropdate,axis=0) # this first scrape was no good, double counted issues and pulls
  except KeyError:
    pass # was already dropped
  return df

class Goal:
  def __init__(self, name, end_date, end_value):
    self.name = name
    self.end_date = end_date
    self.end_value = end_value


def goal_graph(df,title,xlim,ylim,goals,columns):
  colors=default_colors
  ax = df[columns].plot(title=title,xlim=xlim,ylim=ylim,color=colors,lw=3)
  actual_xlim=[ax.get_xlim()[0],ax.get_xlim()[-1]]
  for idx,goal in enumerate(goals):
    plt.annotate(" %s Goal" % goal.name, (goal.end_date, goal.end_value),color=colors[idx])
    y_goal_start = 0
    for i in range(0,len(df[goal.name])):
      if not pd.isnull(df[goal.name][i]):
        y_goal_start = df[goal.name][i]
        break
    yvalues=(y_goal_start,goal.end_value)
    plt.plot(actual_xlim, yvalues,label=None,color=colors[idx],linestyle='--')
  plt.legend(loc='lower right')
  return plt
