# Dissecting BFT Consensus: In Trusted Components we Trust: Reproducibility Document

In this document we explain how to run the resilientDB code and reproduce the results in the paper.


## Setup code and machines
The code is available at [link](https://github.com/msadoghi/resdb-sgx-eurosys). This is a branch of [ResilientDB](https://resilientdb.com/) which is open-source. In order to reproduce results in the paper you need to have:
- A machine to build and deploy (**Dev-Machine**)
- A set of machines to run as replicas and clients 

The dev-machine needs to have the source code, resilientdb dependencies, and SGX dependencies. 
If you have access to oracle cloud machines, we have created images for both of these machines for you and they are ready to use. We will explain later in this document. you also can setup the code and dependencies on your machine or your cloud machines which require a bit more work. 


### Setup without OCI images
1. Clone the code base from this [Repo](https://github.com/msadoghi/resdb-sgx-eurosys)
2. Unzip all the dependencies using this command 
    ```
    cd deps; \ls | xargs -n 1 tar -xvf
    ```
3. install Intel SGX dependencies from [Here](https://github.com/intel/linux-sgx): this requires building and installing 
4. For the server machines we need to only install SGX dependencies and have a directory called `resilientdb` in home directory
### Setup with OCI images
For this, you need to have access to OCI console and create your dev and server machines using the following steps:
 


 1. Create images from the above links. [Oracle guide](https://docs.oracle.com/en-us/iaas/Content/Compute/Tasks/imageimportexport.htm#Importing). The following are Object Storage URLs for machines
    - Dev-Machine:      [Image-Link](https://objectstorage.us-phoenix-1.oraclecloud.com/p/jGPwGQ_jBBTtbtnKBSrP5EUoOE1HRSI2Q3WAWzdJ2F42lvMSZ9EcbbhmA0AYPwcM/n/ax8oq4eg8tc3/b/expo_bucket/o/eurosys-dev-image)
    - Server-Machines:  [Image-Link](https://objectstorage.us-phoenix-1.oraclecloud.com/p/UTEX-ZOq5ovW31Inn_1yrTLS7hW9Tj4Gx4Hhx7Sfpq_a42p8PA2SgrUzWsSKyIwM/n/ax8oq4eg8tc3/b/expo_bucket/o/eurosys-machine-image)
2. Create one Dev-Machine and as many as you need server machines (based on the scale of experiments you need to reproduce) using [Oracle guide](https://docs.oracle.com/en-us/iaas/Content/Compute/Tasks/launchinginstance.htm#linux__linux-create)


---

## Steps to Run and Compile 
* Create **obj** folder inside **resilientdb** folder, to store object files. And **results** to store the results.

        mkdir obj
        mkdir results
        
* We provide a script **startResilientDB.sh** to compile and run the code. To run **ResilientDB** on a cluster such as AWS, Azure or Google Cloud, you need to specify the **Private IP Addresses** of each replica. 
* The code will be compiled on the machine that is running the **startResilientDB.sh** and sends the binary files over SSH to the **resilientdb** folder in all other  nodes. the directory which contains the **resilientdb** in nodes should be set as ``home_directory`` in the following files as:
    1. scripts/scp_binaries.sh
    2. scripts/scp_results.sh
    3. scripts/simRun.py
* **change the ``CNODES`` and ``SNODES`` arrays in ``scripts/startResilientDB.sh`` and put IP Addresses.**
* Adjust the parameters in ``config.h`` such as the number of replicas and clients
* Run script as: 
```
    ./scripts/startResilientDB.sh <number of servers> <number of clients> <result name>

# for example:
    ./scripts/startResilientDB.sh 4 1 PBFT 
```
for the above example, your `startResilientDB.sh` file need to have 4 IP addresses in `SNODES` and 1 IP address in `CNODES`

* All the results after running the script will be stored inside the **results** folder.


#### What is happening behind the scenes?

* The code is compiled using the command: **make clean; make**
* On compilation, two new files are created: **runcl** and **rundb**.
* Each machine that is going to act as a client needs to execute **runcl**.
* Each machine that is going to act as a replica needs to execute **rundb**. 
* The script runs each binary as: **./rundb -nid\<numeric identifier\>**
* This numeric identifier starts from **0** (for the primary) and increases as **1,2,3...** for subsequent replicas and clients.


---

## Configurations for different Protocols

Before running `startResilientDB.sh` script you need to adjust `config.h` file to run the proper protocol you want. There are a couple of parameters in that file you need to change:
 - **NODE_CNT**: which is the number of replicas 
 - **CLIENT_NODE_CNT**: which is the number of clients (throughout the most of experiments this was set to 8) 
 - **SGX**: in order to run any protocol that uses hardware assist, you need to set this to true(all protocols except PBFT and Zyzzyva)
 - The following table shows which parameter to set to true for each protocol: 

| Syntax      | Description |
| ----------- | ----------- |
| **Protocol Name**  | **Config File Parameter** |
| PBFT-EA            | A2M                       |
| MinBFT             | MinBFT                    |
| MinZZ              | MinZZ                     |
| OPBFT-EA           | CONTRUST + A2M            |
| Flexi-BFT          | FLEXI_PBFT                |
| Flexi-ZZ           | FLEXI_ZZ                  |
| PBFT               | PBFT                      |
| ZYZZYVA            | ZYZZYVA                   |

### Sample Config Files

For each protocol we have created a sample config file that you only need to change the number of nodes and clients for them to run that specific protocol.


---

## Contact:

Please contact Sajjad Rahnama (srahnama@ucdavis.edu) and Suyash Gupta (suyash.gupta@berkeley.edu) if you had any questions.
