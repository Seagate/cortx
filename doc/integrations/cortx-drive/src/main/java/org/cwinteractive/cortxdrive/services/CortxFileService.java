package org.cwinteractive.cortxdrive.services;

import java.io.File;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

import org.cwinteractive.cortxdrive.models.FileInputModel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import com.amazonaws.services.s3.AmazonS3;
import com.amazonaws.services.s3.model.ObjectListing;
import com.amazonaws.services.s3.model.ObjectMetadata;
import com.amazonaws.services.s3.model.PutObjectRequest;
import com.amazonaws.services.s3.model.PutObjectResult;
import com.amazonaws.services.s3.model.S3Object;
import com.amazonaws.services.s3.model.S3ObjectInputStream;

@Service
public class CortxFileService {
	
	Logger logger = LoggerFactory.getLogger(CortxFileService.class);
	
	@Value("${cortx.defaultBucketName}")
	protected String defaultBucketName;
	
	private AmazonS3 cortxS3client;
	
	public CortxFileService(AmazonS3 cortxS3client) {
		this.cortxS3client = cortxS3client;
	}
	
	public List<String> getBuckets() {		
		return cortxS3client.listBuckets().stream().map(b -> b.getName()).collect(Collectors.toList());
	}
	
	public String save(File file) {
		PutObjectResult result = cortxS3client.putObject(defaultBucketName, file.getName(), file);
		return result.toString();
	}
	
	public String save(InputStream inputStream, String fileName, FileInputModel fileInputModel) {
		logger.info(String.format("Uploading %s to bucket %s", fileName, defaultBucketName));
		
		// new PutObjectRequest(defaultBucketName, fileName, null)
		
		ObjectMetadata objectMetadata = new ObjectMetadata();
		objectMetadata.addUserMetadata("name", fileInputModel.getName());
		objectMetadata.addUserMetadata("description", fileInputModel.getDescription());
		
		PutObjectResult result = cortxS3client.putObject(defaultBucketName, fileName, inputStream, objectMetadata);
		return result.toString();
	}
	
	public File retrieve(String fileName) throws Exception {
		S3Object s3object = cortxS3client.getObject(defaultBucketName, fileName);
		S3ObjectInputStream inputStream = s3object.getObjectContent();
		String fileNameSuffix = fileName.substring(fileName.lastIndexOf('.'));
		String fileNamePrefix = fileName.substring(0, fileName.lastIndexOf('.'));
		File tempFile = File.createTempFile(fileNamePrefix, fileNameSuffix);
		Files.copy(inputStream, Paths.get(tempFile.getAbsolutePath()), StandardCopyOption.REPLACE_EXISTING);
		return tempFile;
	}
	
	public InputStream retrieveAsStream(String fileName) throws Exception {
		S3Object s3object = cortxS3client.getObject(defaultBucketName, fileName);
		S3ObjectInputStream inputStream = s3object.getObjectContent();
		
		return inputStream;
	}
	
	/*
	public List<FileInputModel> listFilesWithMetadata(String bucketName) {
		ObjectListing objectListing = cortxS3client.listObjects(defaultBucketName);
		
		return objectListing.getObjectSummaries().stream().map(obj -> {
			var fileModel = new FileInputModel();
			S3Object s3Object = cortxS3client.getObject(defaultBucketName, obj.getKey());
			var s3objectUserMetaData = s3Object.getObjectMetadata().getUserMetadata();
			fileModel.setName(s3objectUserMetaData.get("name"));
			fileModel.setDescription(s3objectUserMetaData.get("description"));
			return fileModel;
		}).collect(Collectors.toList());
	}
	
	public List<FileInputModel> listFilesWithMetadata() {
		return listFilesWithMetadata(defaultBucketName);
	}
	
	*/
	
	public List<Map<String, String>> listFilesWithMetadata(String bucketName) {
		ObjectListing objectListing = cortxS3client.listObjects(defaultBucketName);
		
		return objectListing.getObjectSummaries().stream().map(obj -> {
			
			S3Object s3Object = cortxS3client.getObject(defaultBucketName, obj.getKey());
			var s3objectUserMetaData = s3Object.getObjectMetadata().getUserMetadata();
			
			return s3objectUserMetaData;
		}).collect(Collectors.toList());
	}
	
	public List<Map<String, String>> listFilesWithMetadata() {
		return listFilesWithMetadata(defaultBucketName);
	}
	
	public List<String> listFiles(String bucketName) {
		return cortxS3client.listObjects(bucketName).getObjectSummaries().stream().map(o -> o.getKey()).collect(Collectors.toList());
	}
	
	public List<String> listFiles() {
		return listFiles(defaultBucketName);
	}
	
}
