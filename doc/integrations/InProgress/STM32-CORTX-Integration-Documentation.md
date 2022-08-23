# STM32-CORTX Integration

## Introduction

Reading about CORTX's features made me realize the significance of object storage. It is faster in data retrieval and storage, optimized for mass capacity storage, and it is more scalable than file storage despite its cons. IoT is a benefit that will automate and mitigate mundane and difficult task, and curtail environmental detriments of current urban settings, and it will involve large data volumes sent from and to numerous sensors installed and applied to any appliances people need. STMicroelectronics manufactures sensors with efficiency in power and simplification of IoT development and applications. All these comprise a viable digital technology stack, and will help make IoT more manageable, and quicker to operate.

This integration to CORTX involves a Python notebook that would store data streamed from an STM32 device, namely a SensorTile.box, and view its contents after downloading it from the CORTX S3 bucket on AWS. The integration relies on a Bluetooth connection between your desktop and STM32 device. The solution is still under development, as failures are present.

## Installation

### Current Solution

Alongside the Python notebook file, there is a .csv file that was obtained from the ST BLE sensor app. All you need to do is open the notebook file and run it to see it store the .csv file and view the file’s contents after downloading it.

### Intended Solution

You need an STM32 device, such as a SensorTile.box or similar that has an STM32 MCU. Ensure sure that the device is sufficiently charged.  
Next, ensure that Bluetooth is activated on your desktop.  
Then, create an AWS admin account to put in the access key and secret access key. run the notebook on any platform of your choice, including Jupyter Notebook and Google Colab. Ideally, you should view the contents of the .csv file created and stored onto the S3 bucket.

## Video Link

YouTube: [https://youtu.be/YP9wC-81WnI](https://youtu.be/YP9wC-81WnI)
