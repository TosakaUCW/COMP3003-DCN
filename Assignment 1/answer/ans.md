Author: Bohan YANG 2330016056

### 1. What is the OSI model?

#### (1) Explain the different layers in the OSI model:

The OSI model consists of the following seven layers (from top to bottom):

1. **Application Layer (Layer 7)**: This is where end-user applications interact with the network. It includes protocols like HTTP, FTP, and SMTP.
2. **Presentation Layer (Layer 6)**: Responsible for data translation, encryption, and compression. It ensures that data is in a readable format.
3. **Session Layer (Layer 5)**: Manages sessions between applications, ensuring communication is established, maintained, and terminated properly.
4. **Transport Layer (Layer 4)**: Ensures reliable data transfer between systems. It uses protocols like TCP and UDP for flow control, error correction, and segmentation.
5. **Network Layer (Layer 3)**: Manages routing and addressing, determining the best path for data transmission. IP (Internet Protocol) is a key protocol here.
6. **Data Link Layer (Layer 2)**: Responsible for node-to-node data transfer, error detection, and access to the physical medium. Protocols include Ethernet and PPP.
7. **Physical Layer (Layer 1)**: Deals with the physical connection between devices, including the transmission of raw data over cables, fiber, or wireless signals.

#### (2) Compare the differences between the OSI and TCP/IP models:

- **Number of Layers**: The OSI model has 7 layers, while the TCP/IP model has 4 layers (Application, Transport, Internet, and Link).
- **Layer Functions**: OSI is a more theoretical model. But TCP/IP is more practical and is the actual framework used in most modern networking.
- **Standardization**: The OSI model is not directly implemented in networks, while the TCP/IP model is the basis of the internet and modern networks.

### 2. What are the advantages of using fiber-optic cabling rather than copper cabling?

- **Higher Bandwidth**: Fiber-optic cables can transmit data at higher speeds and over longer distances without degradation.
- **Immunity to Interference**: Fiber-optic cables are not susceptible to electromagnetic interference, unlike copper cables.
- **Security**: Fiber-optic cables are more secure because it is harder to tap into the transmission compared to copper cables.
- **Lower Attenuation**: Fiber-optic cables experience less signal loss over long distances.
- **Lightweight and Compact**: Fiber-optic cables are thinner and lighter compared to copper cables.

### 3. **Designing a Computer Network for a Company with 1000 Staff**

#### (1) **Which network topology will you adopt?**

I would adopt a **star topology**. 

In a star topology, all devices are connected to a central hub or switch. This topology is easy to manage and troubleshoot, and failures in one device or connection do not affect the entire network.

#### (2) **What components will be used in the network?**

Key components in the network include:
- **Router**: To connect the company network to the internet or other networks.
- **Switches**: To connect various devices within the network and manage data traffic efficiently.
- **Cabling**: Fiber-optic or Ethernet (CAT 5/6) cables to connect the devices.
- **Access Points**: For wireless connectivity for mobile and remote devices.
- **Firewalls**: For network security and controlling incoming and outgoing traffic.
- **Servers**: For hosting files, applications, and other network services.

#### (3) **Give five important factors when designing a computer network (explain each):**

1. **Scalability**: The network should be designed to accommodate future growth in terms of devices and users.
2. **Reliability**: The network should ensure high availability and minimal downtime, which may involve backup solutions and redundant paths.
3. **Performance**: The network should be able to handle the expected traffic and provide adequate speed for users to perform their tasks.
4. **Security**: The design should include measures like firewalls, encryption, and secure access control to protect sensitive data.
5. **Cost**: The network design should align with the company’s budget, considering installation, maintenance, and potential upgrades.

#### (4) **Use a figure to illustrate the overall design:**

![Figure_1](<Figure_1.png>)

### 5. **Which type of network cable contains multiple copper wires and uses extra shielding to prevent interference?**

The type of network cable that contains multiple copper wires and uses extra shielding to prevent interference is **Shielded Twisted Pair (STP)** cable. 

**Explanation**:
- STP cables have individual shielding around each pair of wires or an overall shield around all the wires. This shielding helps protect the data from electromagnetic interference (EMI) or radio-frequency interference (RFI), ensuring that the signal remains clear and reduces the chances of data loss or corruption. 
- This is in contrast to **Unshielded Twisted Pair (UTP)**, which lacks the shielding and is more susceptible to interference.

### 6. **CRC Method for Error Detection**

Given:
- **Bit stream**: 1101 1111
- **Generator polynomial**: $x^3 + x^2 + 1$ (equivalent to 1101)

#### Step 1: Append 3 zeros (since the degree of the generator polynomial is 3) to the bit stream:
- **Original bit stream**: 1101 1111
- **Appended with zeros**: 1101 1111 000

#### Step 2: Perform binary division (XOR) between the bit stream and the generator polynomial:

- Perform the division of 1101 1111 000 by the divisor 1101.
  
#### Result: The remainder after the division is **111**. This is the CRC value.

#### Step 3: The transmitted bit string (data + CRC) is:
- **1101 1111 000 + CRC (111)** = **1101 1111 111**

Now, if the third bit from the left is inverted during transmission, the received bit string would be:
- **1111 1111 111** (with error in the third bit).

#### Step 4: Error detection at the receiver's end:
- Perform the same division on the received bit string (**1111 1111 001**) by the same generator polynomial. 
- The remainder after division will be non-zero (**010**), indicating an error.

### 7. **Sketch the Manchester Encoding for the bit-stream: 1101 1010**

For the bit stream **1101 1010**, the encoding would look like:

- 1 → High to Low
- 1 → High to Low
- 0 → Low to High
- 1 → High to Low
- 1 → High to Low
- 0 → Low to High
- 1 → High to Low
- 0 → Low to High

The Manchester encoding would be visualized as alternating transitions (clockwise and counterclockwise) corresponding to the bits.

![Figure_2](<Figure_2.png>)

### 8. **Calculate the Hamming Code for the Bit-Stream: 1111 1110 0110**

The Hamming code is used for error detection and correction. Here's how to calculate the Hamming code:

1. **Bit stream**: 1111 1110 0110
2. **Calculate number of parity bits**: For a bit stream of length $m$, the number of parity bits $p$ is determined by the smallest $p$ such that $2^p \geq m + p + 1$. Here, $m = 12$, so we need 5 parity bits ($2^5 = 32 \geq 12 + 5 + 1$).
3. **Insert parity bits in positions**: The parity bits are inserted at positions 1, 2, 4, 8, 16. 
4. **Calculate parity bits**:

| Pos  | Value | Rewrite Pos as sum of power 2 |
| :--- | :---- | :---------------------------- |
| 1    | 0     | -                             |
| 2    | 1     | -                             |
| 3    | 1     | 2 + 1                         |
| 4    | 1     | -                             |
| 5    | 1     | 4 + 1                         |
| 6    | 1     | 4 + 2                         |
| 7    | 1     | 4 + 2 + 1                     |
| 8    | 1     | -                             |
| 9    | 1     | 8 + 1                         |
| 10   | 1     | 8 + 2                         |
| 11   | 1     | 8 + 2 + 1                     |
| 12   | 0     | 8 + 4                         |
| 13   | 0     | 8 + 4 + 1                     |
| 14   | 1     | 8 + 4 + 2                     |
| 15   | 1     | 8 + 4 + 2 + 1                 |
| 16   | 0     | -                             |
| 17   | 0     | 16 + 1                        |


**Answer: 0111 1111 1110 0110 0** 

### 9. **Why Some Applications Use Error Correction Instead of Error Detection and Retransmission**

- **Real-time applications**: In time-sensitive applications (e.g., video streaming, voice calls), we cannot wait for retransmission, so error correction ensures the data is usable without delays.
- **Network Efficiency**: In networks where retransmitting data is costly or inefficient, it can save bandwidth and reducing overhead.

### 10. **Primary Function of the Physical Layer in the OSI Model**

The primary function of the **Physical Layer** is to transmit raw data bits over a physical medium (like cables, fiber optics, or wireless channels). It defines the electrical, mechanical, and procedural aspects of data transmission, including voltage levels, timing, and data rates.

### 11. **How the Physical Layer Handles Data Encoding and Why It Is Important**

The **Physical Layer** handles data encoding by converting the data into signals that can be transmitted over a physical medium. This involves converting binary data (0s and 1s) into electrical, optical, or radio signals that can travel over cables or airwaves. 

This is important because proper encoding ensures the data is transmitted efficiently, and it helps prevent errors during transmission (e.g., from noise or interference).

### 12. **Propose an IoT Application for a Software House**

I would propose a **Smart Warehouse Management System**. This IoT application would use sensors to track inventory in real-time, optimize stock levels, and automate replenishment orders. 

The system would integrate with existing business software, providing real-time analytics, inventory tracking, and alerts for stockouts or overstocking. The benefits include:
- **Increased efficiency**: Automated inventory management reduces human errors and improves stock control.
- **Cost savings**: Real-time insights help prevent overstocking and reduce waste.
- **Enhanced visibility**: Managers can monitor inventory levels from any location, leading to better decision-making.