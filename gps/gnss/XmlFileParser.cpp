/*
Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#define LOG_NDEBUG 0
#define LOG_TAG "LocSvc_XmlParser"
#include <dlfcn.h>
#include "XmlFileParser.h"
#include "log_util.h"
#include <loc_misc_utils.h>

#define LIB_XML_NAME "libxml2.so.2"
#define ECDSA_KEY_LENGTH 67
#define HASH_FUNC_LENGTH 16
#define PK_TYPE_LENGTH   32
#define UHASH_LENGTH     32


// convert character of Hexadecimal into Decimal
uint8_t toDecimal(char ecdsaKey[], int i) {
    uint8_t decVal = 0;
    if (ecdsaKey[i] >= '0' && ecdsaKey[i] <= '9') {
        decVal = ecdsaKey[i] - '0';
    } else if (ecdsaKey[i] >= 'A' && ecdsaKey[i] <= 'Z') {
        decVal = ecdsaKey[i] - 'A' + 10;
    } else if (ecdsaKey[i] >= 'a' && ecdsaKey[i] <= 'z') {
        decVal = ecdsaKey[i] - 'a' + 10;
    }
    return decVal;
}

int loc_read_conf_xml(const char* buffer, int bufLen,
        mgpOsnmaPublicKeyAndMerkleTreeStruct* merkle_tree) {
    xmlChar XML_NODE_SIGNAL_DATA[]         =  "signalData";
    LOC_LOGd("merkle file buffer, length: %d",  bufLen);
    if (NULL == buffer) {
        LOC_LOGe("empty config buffer");
        return -1;
    }
    XmlParserInterface xmlParserIface;
    void* libHandle = nullptr;
    xmlParserIface.xmlParseMemoryFunc = (xmlParseMemoryFuncT)dlGetSymFromLib(
            libHandle, LIB_XML_NAME, "xmlParseMemory");
    xmlParserIface.xmlDocGetRootElementFunc = (xmlDocGetRootElementFuncT)dlGetSymFromLib(
            libHandle, LIB_XML_NAME, "xmlDocGetRootElement");
    xmlParserIface.xmlFreeDocFunc = (xmlFreeDocFuncT)dlGetSymFromLib(libHandle, LIB_XML_NAME,
            "xmlFreeDoc");
    xmlParserIface.xmlCleanupParserFunc = (xmlCleanupParserFuncT)dlGetSymFromLib(
            libHandle, LIB_XML_NAME, "xmlCleanupParser");
    xmlParserIface.xmlStrncmpFunc = (xmlStrncmpFuncT)dlGetSymFromLib(libHandle, LIB_XML_NAME,
            "xmlStrncmp");
    xmlParserIface.xmlStrlenFunc = (xmlStrlenFuncT)dlGetSymFromLib(libHandle, LIB_XML_NAME,
            "xmlStrlen");
    xmlParserIface.xmlNodeGetContentFunc = (xmlNodeGetContentFuncT)dlGetSymFromLib(libHandle,
            LIB_XML_NAME, "xmlNodeGetContent");
    if (!xmlParserIface.xmlParseMemoryFunc || !xmlParserIface.xmlDocGetRootElementFunc ||
            !xmlParserIface.xmlFreeDocFunc || !xmlParserIface.xmlCleanupParserFunc ||
            !xmlParserIface.xmlStrncmpFunc || !xmlParserIface.xmlStrlenFunc ||
            !xmlParserIface.xmlNodeGetContentFunc) {
        LOC_LOGe("fail to load libxml2 API");
        return -2;
    }
   xmlDoc *doc = xmlParserIface.xmlParseMemoryFunc(buffer, bufLen);
   if (!doc) {
       LOC_LOGe("cannot parse the xml file");
       return -3;
   }
   xmlNode *root_element = xmlParserIface.xmlDocGetRootElementFunc(doc);
   if (!root_element) {
       LOC_LOGe("cannot find the root element scan xml file");
       xmlParserIface.xmlFreeDocFunc(doc);
       return -4;
   }

   //Ensure Parent node is <scan> node only
   if ((root_element->type == XML_ELEMENT_NODE) &&
           (xmlParserIface.xmlStrncmpFunc(root_element->name, XML_NODE_SIGNAL_DATA,
                       xmlParserIface.xmlStrlenFunc(XML_NODE_SIGNAL_DATA)) == 0)) {
       parse_scan_elements(root_element->children, merkle_tree, xmlParserIface);
   }
   /*free the document */
   xmlParserIface.xmlFreeDocFunc(doc);

   /*
    *Free the global variables that may
    *have been allocated by the parser.
    */
   xmlParserIface.xmlCleanupParserFunc();
   if (libHandle) {
       dlclose(libHandle);
       memset(&xmlParserIface, 0, sizeof(xmlParserIface));
   }
   return 0;
}

/**
 * Below is the template of the Merkle Tree config xml file.
 *<signalData>
 * <header>
 ...
 * </header>
 * <body>
 *  <MerkleTree>
 *   <N>16</N>
 *   <HashFunction>SHA-256</HashFunction>
 *   <PublicKey>
 *    <i>0>/i>
 *    <PKID>1</PKID>
 *    <lengthInBits>264</lengthInBits>
 *    <point>03F90DB0BE6BDF750835B1017A3A6084CBCB240928AEEFDBC19D1ACA99A3E90899</point>
 *    <PKType>ECDSA P-256/SHA-256</PKType>
 *   </PublicKey>
 *   <PublicKey>
 *    ...
 *   </PublicKey>
 *   <TreeNode>
 *    <i>0</i>
 *    <PKID>1</PKID>
 *    <lengthInBits>264</lengthInBits>
 *    <point>03F90DB0BE6BDF750835B1017A3A6084CBCB240928AEEFDBC19D1ACA99A3E90899</point>
 *    <PKType>ECDSA P-256/SHA-256</PKType>
 *   </TreeNode>
 *   <TreeNode>
 *    ...
 *   </TreeNode>
 *   <TreeNode>
 *    ...
 *   </TreeNode>
 *   <TreeNode>
 *    ...
 *   </TreeNode>
 *   <TreeNode>
 *    ...
 *   </TreeNode>
 *   <TreeNode>
 *    ...
 *   </TreeNode>
 *  </MerkleTree>
 * </body>
 *</signalData>
*/
void parse_scan_elements(xmlNode *a_node,
        mgpOsnmaPublicKeyAndMerkleTreeStruct* merkle_tree, XmlParserInterface& xmlParser) {
    xmlChar XML_NODE_HEADER[]              =  "header";
    xmlChar XML_NODE_BODY[]                =  "body";
    xmlChar XML_NODE_MERKLE_TREE[]         =  "MerkleTree";
    xmlChar XML_NODE_N[]                   =  "N";
    xmlChar XML_NODE_HASH_FUNCTION[]       =  "HashFunction";
    xmlChar XML_NODE_PUBLIC_KEY[]          =  "PublicKey";
    xmlChar XML_NODE_TREE_NODE[]           =  "TreeNode";
    xmlNode *cur_node = NULL;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node != nullptr && cur_node->type == XML_ELEMENT_NODE) {
            if (!xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_HEADER,
                    xmlParser.xmlStrlenFunc(XML_NODE_HEADER))) {
                continue;
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_BODY,
                    xmlParser.xmlStrlenFunc(XML_NODE_BODY)) == 0) {
                parse_scan_elements(cur_node->children, merkle_tree, xmlParser);
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_MERKLE_TREE,
                    xmlParser.xmlStrlenFunc(XML_NODE_MERKLE_TREE)) == 0) {
                parse_scan_elements(cur_node->children, merkle_tree, xmlParser);
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_N,
                    xmlParser.xmlStrlenFunc(XML_NODE_N)) == 0) {
                continue;
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_HASH_FUNCTION,
                    xmlParser.xmlStrlenFunc(XML_NODE_HASH_FUNCTION)) == 0) {
                char hashFunc[HASH_FUNC_LENGTH + 1];
                int status = parse_string_element(cur_node, hashFunc, HASH_FUNC_LENGTH + 1,
                        XML_NODE_HASH_FUNCTION, xmlParser);
                if (status == 0) {
                    if (strncmp(hashFunc, "SHA-256", strlen("SHA-256")) == 0) {
                        merkle_tree[0].zMerkleTree.eHfType = MGP_OSNMA_HF_SHA_256;
                    } else if (strncmp(hashFunc, "SHA3-256", strlen("SHA3-256")) == 0) {
                        merkle_tree[0].zMerkleTree.eHfType = MGP_OSNMA_HF_SHA3_256;
                    }
                }
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_PUBLIC_KEY,
                    xmlParser.xmlStrlenFunc(XML_NODE_PUBLIC_KEY)) == 0) {
                int keyCount = 1;
                while (cur_node->next != nullptr &&  (xmlParser.xmlStrncmpFunc(cur_node->next->name,
                        XML_NODE_PUBLIC_KEY, xmlParser.xmlStrlenFunc(XML_NODE_PUBLIC_KEY)) == 0)) {
                    cur_node = cur_node->next;
                    ++keyCount;
                }
                // there might be three or more public keys, inject the last two
                if (keyCount > 1) {
                    parse_public_key_elements(cur_node->children, merkle_tree[1].zPublicKey,
                            xmlParser);
                    merkle_tree[1].zPublicKey.uFlag = 1;
                    cur_node = cur_node->prev;
                    parse_public_key_elements(cur_node->children, merkle_tree[0].zPublicKey,
                            xmlParser);
                    merkle_tree[0].zPublicKey.uFlag = 1;
                    cur_node = cur_node->next;
                } else { // only one public key
                    parse_public_key_elements(cur_node->children, merkle_tree[0].zPublicKey,
                            xmlParser);
                    merkle_tree[0].zPublicKey.uFlag = 1;
                }
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_TREE_NODE,
                    xmlParser.xmlStrlenFunc(XML_NODE_TREE_NODE)) == 0) {
                parse_tree_node_elements(cur_node->children, merkle_tree[0].zMerkleTree.zRootNode,
                        xmlParser);
                merkle_tree[0].zMerkleTree.uFlag = 1;
                vector<mgpOsnmaTreeNodeT> treeNodes; //save all the tree nodes except root node
                while (cur_node->next != nullptr && (xmlParser.xmlStrncmpFunc(cur_node->next->name,
                        XML_NODE_TREE_NODE, xmlParser.xmlStrlenFunc(XML_NODE_TREE_NODE)) == 0)) {
                    mgpOsnmaTreeNodeT node;
                    cur_node = cur_node->next;
                    parse_tree_node_elements(cur_node->children, node, xmlParser);
                    treeNodes.push_back(node);
                }
                IF_LOC_LOGV {
                    for (int i = 0; i < treeNodes.size(); ++i) {
                        LOC_LOGv("treeNodes[%d]: j, %d i, %d", i, treeNodes[i].uj, treeNodes[i].ui);
                    }
                }
                vector<vector<int>> treePath(2); // value i of all the 4 required Merkle tree nodes

                if (merkle_tree[0].zPublicKey.uNpkId == 0) {
                    LOC_LOGi("OSNMA Alert Message! No need to populate the treePath.");
                    break;
                }
                get_node_path(0, merkle_tree[0].zPublicKey.uNpkId-1,
                        merkle_tree[0].zMerkleTree.zRootNode.uj,
                        merkle_tree[0].zMerkleTree.zRootNode.ui, treePath[0]);
                int keyCnt = merkle_tree[1].zPublicKey.uFlag == 1? 2: 1;
                IF_LOC_LOGV {
                    for (int i = 0; i < treePath[0].size(); ++i) {
                        LOC_LOGv("treePath[0][%d]: %d", i, treePath[0][i]);
                    }
                }

                // copy MerkleTreeT to 2nd mgpOsnmaPublicKeyAndMerkleTreeStruct
                if (keyCnt == 2) {
                    merkle_tree[1].zMerkleTree = merkle_tree[0].zMerkleTree;
                    get_node_path(0, merkle_tree[1].zPublicKey.uNpkId-1,
                            merkle_tree[1].zMerkleTree.zRootNode.uj,
                            merkle_tree[1].zMerkleTree.zRootNode.ui, treePath[1]);
                    IF_LOC_LOGV {
                        for (int i = 0; i < treePath[1].size(); ++i) {
                            LOC_LOGv("treePath[1][%d]: %d",  i, treePath[1][i]);
                        }
                    }
                }
                for (int keyNum =0; keyNum < keyCnt; ++keyNum) {
                    for (int layer=0; layer < 4; ++layer) {
                        merkle_tree[keyNum].zPublicKey.zNodes[layer] =
                                find_node(layer, treePath[keyNum][layer], treeNodes);
                        LOC_LOGd("merkle_tree[%d].zPublicKey.zNodes[%d]: j, %d i, %d", keyNum,
                            layer, merkle_tree[keyNum].zPublicKey.zNodes[layer].uj,
                            merkle_tree[keyNum].zPublicKey.zNodes[layer].ui);
                    }
                }

                break;
            }
        }
    }
}

void parse_public_key_elements(xmlNode *a_node, mgpOsnmaPublicKeyT& publicKey,
        XmlParserInterface& xmlParser) {
    xmlChar XML_NODE_POINT[]               =  "point";
    xmlChar XML_NODE_PK_TYPE[]             =  "PKType";
    xmlChar XML_NODE_PKID[]                =  "PKID";
    xmlChar XML_NODE_LENGTH_IN_BITS[]      =  "lengthInBits";
    xmlChar XML_NODE_I[]                   =  "i";
    xmlNode *cur_node = NULL;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node != nullptr && cur_node->type == XML_ELEMENT_NODE) {
            if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_I,
                    xmlParser.xmlStrlenFunc(XML_NODE_I)) == 0) {
                continue;
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_PKID,
                    xmlParser.xmlStrlenFunc(XML_NODE_PKID)) == 0) {
                int pkID;
                int status = parse_int_element(cur_node, pkID, XML_NODE_PKID, xmlParser);
                if (status == 0) {
                    publicKey.uNpkId = pkID;
                }
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_LENGTH_IN_BITS,
                    xmlParser.xmlStrlenFunc(XML_NODE_LENGTH_IN_BITS)) == 0) {
                int lenInBits;
                int status = parse_int_element(cur_node, lenInBits, XML_NODE_LENGTH_IN_BITS,
                        xmlParser);
                if (status == 0) {
                    publicKey.wKeyLen = lenInBits;
                }
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_POINT,
                    xmlParser.xmlStrlenFunc(XML_NODE_POINT)) == 0) {
                char ecdsaKey[ECDSA_KEY_LENGTH * 2 + 1];
                int status = parse_string_element(cur_node, ecdsaKey, 135, XML_NODE_POINT,
                        xmlParser);
                if (status == 0) {
                    uint8_t ecdsaKeyDec[ECDSA_KEY_LENGTH] = {0};
                    int i = 0;
                    // convert Key from char to decimal
                    for (i = 0; i < ECDSA_KEY_LENGTH && ecdsaKey[2*i] != '\0' &&
                            ecdsaKey[2*i+1] != '\0'; ++i) {
                        ecdsaKeyDec[i] = toDecimal(ecdsaKey, 2*i) * 16 + toDecimal(ecdsaKey, 2*i+1);
                    }
                    if (i < ECDSA_KEY_LENGTH && ecdsaKey[2*i+1] == '\0') {
                        ecdsaKeyDec[i] = toDecimal(ecdsaKey, 2*i) * 16;
                    }
                    memcpy(publicKey.uKey, ecdsaKeyDec, ECDSA_KEY_LENGTH);
                    //For DEBUG
                    IF_LOC_LOGV {
                        for (int j = 0; j < ECDSA_KEY_LENGTH; ++j) {
                            LOC_LOGv("publicKey.uKey[%d]: %d", j, publicKey.uKey[j]);
                        }
                    }
                }
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_PK_TYPE,
                    xmlParser.xmlStrlenFunc(XML_NODE_PK_TYPE)) == 0) {
                char pkType[PK_TYPE_LENGTH + 1];
                int status = parse_string_element(cur_node, pkType, PK_TYPE_LENGTH + 1,
                        XML_NODE_PK_TYPE, xmlParser);
                if (status == 0) {
                    if (strncmp(pkType, "ECDSA P-256", strlen("ECDSA P-256")) == 0) {
                        publicKey.eNpkt = MGP_OSNMA_NPKT_ECDSA_P_256;
                    } else if (strncmp(pkType, "ECDSA P-521", strlen("ECDSA P-521")) == 0) {
                        publicKey.eNpkt = MGP_OSNMA_NPKT_ECDSA_P_521;
                    } else if (strncmp(pkType, "ALERT", strlen("ALERT")) == 0) {
                        publicKey.eNpkt = MGP_OSNMA_NPKT_ALERT;
                    }
                }
            }
        }
    }
    LOC_LOGd("publicKey.uNpkId: %d, publicKey.wKeyLen: %d, eNpkt: %d",
            publicKey.uNpkId, publicKey.wKeyLen, publicKey.eNpkt);
}

void parse_tree_node_elements(xmlNode *a_node, mgpOsnmaTreeNodeT& treeNode,
        XmlParserInterface& xmlParser) {
    xmlChar XML_NODE_J[]                   =  "j";
    xmlChar XML_NODE_X_JI[]                =  "x_ji";
    xmlChar XML_NODE_I[]                   =  "i";
    xmlChar XML_NODE_LENGTH_IN_BITS[]      =  "lengthInBits";
    xmlNode *cur_node = NULL;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node != nullptr && cur_node->type == XML_ELEMENT_NODE) {
            if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_I,
                    xmlParser.xmlStrlenFunc(XML_NODE_I)) == 0) {
                int iVal;
                int status = parse_int_element(cur_node, iVal, XML_NODE_I, xmlParser);
                if (status == 0) {
                    treeNode.ui = iVal;
                }
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_J,
                    xmlParser.xmlStrlenFunc(XML_NODE_J)) == 0) {
                int jVal;
                int status = parse_int_element(cur_node, jVal, XML_NODE_J, xmlParser);
                if (status == 0) {
                    treeNode.uj = jVal;
                }
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_LENGTH_IN_BITS,
                        xmlParser.xmlStrlenFunc(XML_NODE_LENGTH_IN_BITS)) == 0) {
                int lenInBits;
                int status = parse_int_element(cur_node, lenInBits, XML_NODE_LENGTH_IN_BITS,
                        xmlParser);
                if (status == 0) {
                    treeNode.wLengthInBits = lenInBits;
                }
            } else if (xmlParser.xmlStrncmpFunc(cur_node->name, XML_NODE_X_JI,
                    xmlParser.xmlStrlenFunc(XML_NODE_X_JI)) == 0) {
                char xJiVal[UHASH_LENGTH * 2 + 1];
                int status = parse_string_element(cur_node, xJiVal, UHASH_LENGTH * 2 + 1,
                        XML_NODE_X_JI, xmlParser);
                if (status == 0) {
                    uint8_t xJiValDec[UHASH_LENGTH] = {0};
                    int i = 0;
                    // convert hash value from char to decimal
                    for (i=0; i < UHASH_LENGTH && xJiVal[2*i] != '\0'; ++i) {
                       xJiValDec[i] = toDecimal(xJiVal, 2*i) * 16 + toDecimal(xJiVal, 2*i+1);
                    }

                    memcpy(treeNode.uHash, xJiValDec, sizeof(xJiValDec));
                    //For DEBUG
                    IF_LOC_LOGV {
                        for (int j = 0; j < UHASH_LENGTH; ++j) {
                            LOC_LOGv("treeNode.uHash[%d]: %d", j, treeNode.uHash[j]);
                        }
                    }
                }
            }
        }
    }
    LOC_LOGd("treeNode.ui: %d, treeNode.uj: %d, treeNode.wLengthInBits: %d\n",
            treeNode.ui, treeNode.uj, treeNode.wLengthInBits);
}

int parse_int_element(xmlNode *pXmlNode, int& value,
        const xmlChar * const xmlString, XmlParserInterface& xmlParser) {
    int retVal = -1;
    const char *payloadData = (const char *)xmlParser.xmlNodeGetContentFunc(pXmlNode);
    do {
        if (NULL == payloadData) {
            LOC_LOGe("%s No Payload\n",  xmlString);
            break;
        }
        LOC_LOGv("%s Payload: %s\n", xmlString, payloadData);
        int status = sscanf(payloadData, "%d", &value);
        if (status < 1) {
            LOC_LOGe(" Element %s not formed correctly. Value = %d\n", xmlString, value);
            break;
        }
        retVal = 0;
    }
    while (0);
    return retVal;
}

int parse_string_element(xmlNode *pXmlNode, char* p_arr, int len,
        const xmlChar * const xmlString, XmlParserInterface& xmlParser) {
    int retVal = -1;
    const char *payloadData = (const char *)xmlParser.xmlNodeGetContentFunc(pXmlNode);
    do {
        if ( (NULL == p_arr) || (0 == len) ) {
            LOC_LOGe("%s Invalid input params\n", xmlString);
            break;
        }
        if (NULL == payloadData) {
            LOC_LOGe("%s No Payload\n", xmlString);
            break;
        }
        LOC_LOGv("%s Payload: %s\n", xmlString, payloadData);
        len = strlcpy (p_arr, payloadData, len);
        if (len < 1) {
            LOC_LOGe(" Element %s not formed correctly.\n", xmlString);
            break;
        }
        retVal = 0;
    } while (0);
    return retVal;
}

void get_node_path(int j, int i, int rootJ, int rootI, vector<int>& vec) {
    if (j<0 || i<0) {
        return;
    }
    if (j == rootJ) return;
    if (i % 2 == 0) {
        vec.push_back(i+1);
    } else {
        vec.push_back(i-1);
    }
    get_node_path(++j, i/2, rootJ, rootI, vec);
}

mgpOsnmaTreeNodeT find_node(int j, int i, vector<mgpOsnmaTreeNodeT>& vec) {
    for (int n=0; n<vec.size(); ++n) {
        if (j == vec[n].uj && i == vec[n].ui) {
            return vec[n];
        }
    }
    LOC_LOGe(" required Merkle tree nodes (%d, %d) not found.\n", j, i);
    return {};
}
