#! /usr/bin/env python3

import colorsys
import cortx_community
import matplotlib.pyplot as plt
import pandas as pd

# manually copied in from seaborn
default_colors=[(0.12156862745098039, 0.4666666666666667, 0.7058823529411765), (1.0, 0.4980392156862745, 0.054901960784313725), (0.17254901960784313, 0.6274509803921569, 0.17254901960784313), (0.8392156862745098, 0.15294117647058825, 0.1568627450980392), (0.5803921568627451, 0.403921568627451, 0.7411764705882353), (0.5490196078431373, 0.33725490196078434, 0.29411764705882354), (0.8901960784313725, 0.4666666666666667, 0.7607843137254902), (0.4980392156862745, 0.4980392156862745, 0.4980392156862745), (0.7372549019607844, 0.7411764705882353, 0.13333333333333333), (0.09019607843137255, 0.7450980392156863, 0.8117647058823529)]
pastels=[(0.6313725490196078, 0.788235294117647, 0.9568627450980393), (1.0, 0.7058823529411765, 0.5098039215686274), (0.5529411764705883, 0.8980392156862745, 0.6313725490196078), (1.0, 0.6235294117647059, 0.6078431372549019), (0.8156862745098039, 0.7333333333333333, 1.0), (0.8705882352941177, 0.7333333333333333, 0.6078431372549019), (0.9803921568627451, 0.6901960784313725, 0.8941176470588236), (0.8117647058823529, 0.8117647058823529, 0.8117647058823529), (1.0, 0.996078431372549, 0.6392156862745098), (0.7254901960784313, 0.9490196078431372, 0.9411764705882353)]
blacks=['black','red','grey']

# scale_l > 1 will lighten
# scale_l < 1 will darken
def scale_lightness(rgb,scale_l):
  # convert rgb to hls
  h, l, s = colorsys.rgb_to_hls(*rgb)
  # manipulate h, l, s values and return as rgb
  return colorsys.hls_to_rgb(h, min(1, l * scale_l), s = s)

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
    values = ps.get_values_as_numbers(repo,k)
    if 'ave_age_in_s' in k:
      values = [None if v is None else v/86400 for v in values]
      k= k.replace('ave_age_in_s','ave_age_in_days')
    data[k]=values
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


def goal_graph(df,title,xlim,goals,columns,ylim=None):
  colors=default_colors
  # drop nan values to make a better plot
  ax = df[columns].dropna().plot(title=title,xlim=xlim,color=colors,lw=4)
  actual_xlim=[ax.get_xlim()[0],ax.get_xlim()[-1]]
  max_y=0
  for c in columns:
    max_y = max(max_y,df[c].max())
  for idx,goal in enumerate(goals):
    # add a label at the end
    plt.annotate(" %s Goal" % goal.name, (goal.end_date, goal.end_value),color=colors[idx])
    # find the first known value for this goal and use it to start the goal
    y_goal_start = 0
    for i in range(0,len(df[goal.name])):
      if not pd.isnull(df[goal.name][i]):
        y_goal_start = df[goal.name][i]
        break
    yvalues=(y_goal_start,goal.end_value)
    max_y = max(max_y,goal.end_value)
    # a dashed line for the goal
    linestyle=(0,(1,10)) # loosely dotted, might be too faint when combined with a lighter color
    linestyle="dotted"
    color=scale_lightness(colors[idx],1.5) # lighten the color for the goal line
    plt.plot(actual_xlim, yvalues,label=None, color=color, linestyle=linestyle,lw=4)

  if ylim is None:
    ylim=(0,max_y*1.1)  # find the max y-value and pad the graph by 10% over that
  ax.set_ylim(ylim) 
  plt.legend(loc='lower right')
  return plt
