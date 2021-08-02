// Sumit Kumar, 2021-08-02

package main

import (
	"fmt"
	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/credentials"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/s3"
	"github.com/aws/aws-sdk-go/service/s3/s3manager"
	"os"
)

func main() {
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

	//Let's create a new bucket using the CreateBucket call.
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

	//Let's list all the buckets using ListBuckets call
	result, err :=
		s3Client.ListBuckets(nil)
	if err != nil {
		fmt.Println("Unable to list buckets",err.Error())
		return
	}
	// print all found buckets
	fmt.Println("Buckets:")
	for _, b := range result.Buckets {
		fmt.Printf("* %s created on %s\n",
			aws.StringValue(b.Name), aws.TimeValue(b.CreationDate))
	}

	//Let's upload a test object or file to the bucket we created above
	// to upload external file, provide FILENAME as argument to go run s3_utility.go command
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
		fmt.Println("Failed to upload data", err.Error())
		return
	}
	fmt.Printf("Successfully created bucket %s and uploaded data", *bucket)

	// Let's download the uploaded file from the bucket created
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
			Key: aws.String(filename),})
	if err != nil {
		fmt.Println("Failed to download file", err.Error())
		return
	}
	fmt.Println("Downloaded file", file.Name(), numBytes, "bytes")

	// Let's list objects inside the bucket
	// Get the list of items
	resp, err := s3Client.ListObjectsV2(&s3.ListObjectsV2Input{Bucket: bucket})
	if err != nil {
		fmt.Println("Unable to list objects", err.Error())
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

	// Lets' delete object, here, the file we uploaded inside the bucket
	// Delete the item (here, file we uploaded, key was set as filename)
	_, err = s3Client.DeleteObject(&s3.DeleteObjectInput{Bucket: bucket, Key:aws.String(filename)})
	if err != nil {
		fmt.Println("Unable to delete object from bucket", err.Error())
		return
	}
	err = s3Client.WaitUntilObjectNotExists(&s3.HeadObjectInput{
		Bucket: bucket,
		Key: aws.String(filename),
	})
	if err != nil {
		fmt.Println("Error occurred while waiting for object to be deleted", err.Error())
		return
	}
	fmt.Printf("Object %q successfully deleted\n", filename)
	// Let's delete the bucket which we created in beginning
	_, err = s3Client.DeleteBucket(&s3.DeleteBucketInput{Bucket: bucket,})
	if err != nil {
		fmt.Println("Unable to delete bucket", err.Error())
		return
	}
	// Wait until bucket is deleted before finishing
	fmt.Printf("Waiting for bucket %q to be deleted...\n", bucket)
	err = s3Client.WaitUntilBucketNotExists(&s3.HeadBucketInput{Bucket: bucket,})
	if err != nil {
		fmt.Println("Error occurred while waiting for bucket to be deleted", err.Error())
		return
	}
	fmt.Printf("Bucket %q successfully deleted\n", bucket)
} //end of fun main()
