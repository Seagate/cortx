# Goofys/CORTX

## Step-By-Step Video

[![Goofys/CORTX Integration Video](video-thumbnail.png)](https://vimeo.com/581988233)


## What is Goofys?

[goofys](https://github.com/kahing/goofys) lets us mount S3 and S3-compatible storage buckets as a filesystem. 

Hence, using goofys, we are able to create a folder on our own machine that automatically 'syncs' with your CORTX bucket.

## Installation 

We created a [convenience script](./goofys-cortx-setup.sh) that creates a folder `/root/shared` that will be used to mount your CORTX bucket. Simply carry out the following command:

```
sh goofys-cortx-setup.sh <cortx-endpoint-url> <bucket-name> <aws-access-key-id> <secret-access-key>
```

For detailed integration steps, do check out our 5-minute [step-by-step video]() which explains all the steps we have listed in our convenience script.

## Done By

Team Zelda:

- Claire Lim
- Hwang Sung Won
- Tan Ziheng

If there are any questions, do not hesitate to contact our team at clairelimjiaying@ntudsc.com.