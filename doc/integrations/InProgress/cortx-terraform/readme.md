# Terraform Provider - Seagate CORTX


This README refers to an integration between the CORTX API and Hashicorp [Terraform](https://www.terraform.io/). You can refer to the following resources to learn more:

- [Video](https://youtube.com/video/m3MeDl-5jKE)
- [Source + Instructions](https://github.com/DMW2151/big-pile-of-storage)

Infrastructure As Code tools like Terraform can allow organizations to standardize infrastructure and policies across multiple regions, environments, applications, etc. With the this provider, users can now include CORTX resources with their other infrastructure deployments rather than relying on custom scripts or one-off deployment patterns. This provider is opinionated in that it simplifies AWS' provider, abstracts away the elements of the S3 API that aren't applicable for CORTX, and provides a cleaner interface between Terraform and CORTX than the AWS provider can.

## Building The Provider

Terraform providers are [Golang](https://go.dev/dl/) modules. If you've got Go >=v1.17 on your machine, you can run the following command to build the provider locally. Please see notes in the MakeFile, you may need to change the `$TARGET_ARCH` and `$TARGET_OS` arguments to be compatible with your system's operating system and architecture before building.

```bash 
make install
```

## Examples - Using the Provider

Please refer to the README in the Terraform provider repo for a full discussion of examples. For a quick (and free) test, I'd recommend running the CloudShare example. If you're familiar with Terraform, you may simply run `make install`, which will handle building and installing the provider. From there, declaring and initializing the CORTX provider is the same as any other Terraform provider. Consider the example below:

```terraform
// Terraform Main Config //
terraform {

  // Provider Versions
  required_providers {
    cortx = {
      version = "0.0.1"
      source  = "dmw2151.com/terraform/cortx"
    }
  }

  // Terraform Version
  required_version = ">= 1.0.3"

}

// CORTX Provider Config //
provider "cortx" {
  // CORTX Connection Configuration 
  cortx_endpoint_host = "localhost"
  cortx_endpoint_port = "28001"

  // CORTX User Configuration
  cortx_access_key        = "..."
  cortx_secret_access_key = "..."
}
```

