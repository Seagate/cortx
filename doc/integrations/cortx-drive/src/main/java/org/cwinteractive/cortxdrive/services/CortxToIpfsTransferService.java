package org.cwinteractive.cortxdrive.services;

import java.io.File;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class CortxToIpfsTransferService {
	
	Logger logger = LoggerFactory.getLogger(CortxToIpfsTransferService.class);
	
	private CortxFileService cortxFileService;
	
	private IPFSFileService ipfsFileService;
	
	public CortxToIpfsTransferService(CortxFileService cortxFileService, IPFSFileService ipfsFileService) {
		this.cortxFileService = cortxFileService;
		this.ipfsFileService = ipfsFileService;
	}
	
	public Map<String,Object> moveToIPFS(String fileName) throws Exception {
		File cortxObjectFile = cortxFileService.retrieve(fileName);
		logger.info(String.format("Retrieved cortx object: %s, size: %s", cortxObjectFile.getName(), cortxObjectFile.length()));
		Map<String,Object> addResult = ipfsFileService.save(cortxObjectFile);
		return addResult;
	}
	
	public String moveToCortx(String cid, String fileName) throws Exception {
		File file = ipfsFileService.download(cid, fileName);
		String result = cortxFileService.save(file);
		return result;
	}

}
