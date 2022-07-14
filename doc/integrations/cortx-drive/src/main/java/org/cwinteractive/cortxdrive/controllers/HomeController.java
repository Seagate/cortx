package org.cwinteractive.cortxdrive.controllers;

import java.io.IOException;
import java.util.List;
import java.util.Map;

import org.cwinteractive.cortxdrive.models.FileInputModel;
import org.cwinteractive.cortxdrive.models.StatusMessage;
import org.cwinteractive.cortxdrive.services.CortxFileService;
import org.cwinteractive.cortxdrive.services.CortxToIpfsTransferService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.core.io.Resource;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.ui.ModelMap;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.ModelAttribute;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestParam;

import com.fasterxml.jackson.databind.ObjectMapper;

@Controller
public class HomeController {

	Logger logger = LoggerFactory.getLogger(HomeController.class);

	@Autowired
	ObjectMapper objectMapper;
	
	@Autowired
	CortxToIpfsTransferService cortxToIpfsTranserService;

	@Autowired
	CortxFileService cortxFileService;

	@GetMapping("/foundation")
	public String foundationHome() {
		return "foundation";
	}

	@GetMapping("/")
	public String start() {
		return "index";
	}

	@GetMapping("/home")
	public String home(FileInputModel fileInputModel, ModelMap modelMap) {
		modelMap.addAttribute("uploadFileData", fileInputModel);
		var cortxFiles = cortxFileService.listFiles();
		modelMap.addAttribute("cortxFiles", cortxFiles);
		
		// var cortxFilesMetadata = cortxFileService.listFilesWithMetadata();
		modelMap.addAttribute("cortxFilesMetadata", List.of());
		
		return "home";
	}

	@GetMapping("/buckets")
	public String allBuckets(Model model) {
		model.addAttribute("buckets", cortxFileService.getBuckets());
		return "s3tests";
	}

	@PostMapping("/uploadFile")
	public String uploadFile(@ModelAttribute FileInputModel fileInputModel, ModelMap modelMap) throws IOException {
		modelMap.addAttribute("uploadFileData", fileInputModel);
		logger.info("uploading file: " + fileInputModel.getName());
		logger.info("file content type: " + fileInputModel.getFile().getContentType());

		String result = cortxFileService.save(fileInputModel.getFile().getInputStream(),
				fileInputModel.getFile().getOriginalFilename(),
				fileInputModel);
		
		logger.info("Upload to CORTX completed: " + result);
		
		modelMap.addAttribute("statusMessage", 
				new StatusMessage("File uploaded", String.format("Your file %s has been successfully saved to CORTX.", fileInputModel.getFile().getOriginalFilename()), 1));

		var cortxFiles = cortxFileService.listFiles();
		modelMap.addAttribute("cortxFiles", cortxFiles);
		
		return "home";
	}
	
	@GetMapping(path = "/copyToIpfs")
	public String copyToIPFS(@RequestParam("fileName") String fileName, ModelMap modelMap) throws Exception {
		modelMap.addAttribute("uploadFileData", new FileInputModel());
		logger.info(String.format("Copying %s to IPFS ..", fileName));
		
		Map<String, Object> copyToIPFSResponse = cortxToIpfsTranserService.moveToIPFS(fileName);
		
		String cid = copyToIPFSResponse.get("Name").toString();
		
		var cortxFiles = cortxFileService.listFiles();
		modelMap.addAttribute("cortxFiles", cortxFiles);
		
		var statusMessage = new StatusMessage("Copy to IPFS", String.format("File %s saved to IPFS with CID: %s", fileName, cid) , 0);
		modelMap.addAttribute("statusMessage", statusMessage);
		return "home";
	}
	
	@GetMapping(path = "/importFromIpfs")
	public String downloadFromIPFS(@RequestParam("cid") String cid, @RequestParam("fileName") String fileName, Model model) throws Exception {
		String putObjectResult = cortxToIpfsTranserService.moveToCortx(cid, fileName);
		StatusMessage statusMessage = new StatusMessage("File imported to CORTX", putObjectResult, 1);
		model.addAttribute("statusMessage", statusMessage);
		var cortxFiles = cortxFileService.listFiles();
		model.addAttribute("cortxFiles", cortxFiles);
		
		return "home";
	}
	
	
	@GetMapping(path = "/download") 
	Resource downloadFromCORTX(@RequestParam("fileName") String fileName) throws Exception {
		
		return null;
	}

}
