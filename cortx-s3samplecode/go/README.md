CORTX S3 Sample Code - go
==============================

Prerequisites
---------------------
* go 1.15
```
$ go get -u github.com/aws/aws-sdk-go/...
```

Full Script
---------------------
Sample script name - [cortx-s3samplecode-go.go](cortx-s3samplecode-go.go)

Session  Setup
---------------------
```go
// configure Cortx S3 server
// endpoint is your cortex S3 endpoint URL
s3Config := &aws.Config{
Credentials: credentials.NewStaticCredentials("YOUR-ACCESSKEYID", "YOUR-SECRETACCESSKEY", ""),
Endpoint: aws.String("http://localhost:8080"),
Region: aws.String("us-east-1"),
DisableSSL: aws.Bool(true),
S3ForcePathStyle: aws.Bool(true),
}
newSession := session.New(s3Config)
// Create S3 service client
s3Client := s3.New(newSession)
```

Create Bucket
---------------------
```go
bucket := aws.String("test-bucket")
cparams := &s3.CreateBucketInput{
Bucket: bucket, // Required
}
_, err := s3Client.CreateBucket(cparams)
if err != nil {
// Message from an error.
fmt.Println(err.Error())
return
}
fmt.Printf("Successfully created bucket %s\n", *bucket)
```

List Buckets
---------------------
```go
result, err := s3Client.ListBuckets(nil)
if err != nil {
fmt.Println("Unable to list buckets",err)
return
}
// print all found buckets
fmt.Println("Buckets:")
for _, b := range result.Buckets {
fmt.Printf("* %s created on %s\n",
aws.StringValue(b.Name), aws.TimeValue(b.CreationDate))
}
```

Put Object
---------------------
```go
// to upload external file, provide FILENAME as argument to go run s3_utility.go
command
filename := os.Args[1]
objectFile, err := os.Open(filename)
if err != nil {
fmt.Println("Unable to open file", err)
}
defer objectFile.Close()
// Upload the file to our "test-bucket".
_, err = s3Client.PutObject(&s3.PutObjectInput{
Body: objectFile,
Bucket: bucket,
Key: aws.String(filename),
})
if err != nil {
fmt.Printf("Failed to upload data to %s/%s, %s\n", *bucket, *key, err.Error())
return
}
fmt.Printf("Successfully created bucket %s and uploaded data with key %s\n",
*bucket, *key)
```

Get Object
---------------------
```go
// Retrieve our file from our "test-bucket" and store it locally in "file_local"
file, err := os.Create("file_local")
if err != nil {
fmt.Println("Failed to create file", err)
return
}
defer file.Close()
downloader := s3manager.NewDownloader(newSession)
numBytes, err := downloader.Download(file,
&s3.GetObjectInput{
Bucket: bucket,
Key: aws.String(filename),}
)
if err != nil {
fmt.Println("Failed to download file", err)
return
}
fmt.Println("Downloaded file", file.Name(), numBytes, "bytes")
```

List Objects
---------------------
```go
// Get the list of items
resp, err := s3Client.ListObjectsV2(&s3.ListObjectsV2Input{Bucket: bucket})
if err != nil {
fmt.Println("Unable to list objects", err)
return
}
for _, item := range resp.Contents {
fmt.Println("Name: ", *item.Key)
fmt.Println("Last modified:", *item.LastModified)
fmt.Println("Size: ", *item.Size)
fmt.Println("Storage class:", *item.StorageClass)
fmt.Println("")
}
fmt.Println("Found", len(resp.Contents), "items in bucket", bucket)
fmt.Println("")
```

Delete Object
---------------------
```go
// Delete the item (here, file we uploaded, key was set as filename)
_, err = s3Client.DeleteObject(&s3.DeleteObjectInput{Bucket: bucket, Key:
aws.String(filename)}
)
if err != nil {
fmt.Println("Unable to delete object from bucket", err)
return
}
err = s3Client.WaitUntilObjectNotExists(&s3.HeadObjectInput{
Bucket: bucket,
Key: aws.String(filename),
})
if err != nil {
fmt.println("Error occurred while waiting for object to be deleted", err)
}
fmt.Printf("Object %q successfully deleted\n", filename)
```

Delete Bucket
---------------------
```go
_, err = s3Client.DeleteBucket(&s3.DeleteBucketInput{Bucket: bucket,}
)
if err != nil {
fmt.Println("Unable to delete bucket", err)
return
}
// Wait until bucket is deleted before finishing
fmt.Printf("Waiting for bucket %q to be deleted...\n", bucket)
err = s3Client.WaitUntilBucketNotExists(&s3.HeadBucketInput{Bucket: bucket,})
if err != nil {
fmt.Println("Error occurred while waiting for bucket to be deleted", error)
return
}
fmt.Printf("Bucket %q successfully deleted\n", bucket)
```
Troubleshooting
---------------------
If encounter the error below, it means `test-bucket` is created succesfully but an input file to upload is not specified. You can specify a test file as the Go executable's 1st argument.
```
* test-bucket created on 2022-02-27 06:34:00 +0000 UTC
panic: runtime error: index out of range [1] with length 1
```

If encounter the error `BucketAlreadyOwnedByYou: The bucket you tried to create already exists, and you own it. status code: 409`, it means the `test_bucket` is already existing. You can delete the bucket with the command below then re-run.
```
aws s3 --endpoint "YOUR_S3_ENDPOINT" rb s3://test-bucket
```

### Tested By:
* Feb 27, 2022: Bo Wei (bo.b.wei@seagate.com) using Cortx OVA 2.0.0 as S3 Server.

