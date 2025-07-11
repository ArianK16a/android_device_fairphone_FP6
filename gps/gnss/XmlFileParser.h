/*
Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef XML_FILE_PARSER_H
#define XML_FILE_PARSER_H
#include <stdint.h>
#include <sys/types.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <vector>
#include "LocationDataTypes.h"
using namespace std;

typedef xmlDoc* (*xmlParseMemoryFuncT)(const char*, int);
typedef xmlNode* (*xmlDocGetRootElementFuncT)(xmlDoc*);
typedef void (*xmlFreeDocFuncT)(xmlDocPtr);
typedef void (*xmlCleanupParserFuncT)();
typedef int (*xmlStrncmpFuncT)(const xmlChar*, const xmlChar*, int);
typedef int (*xmlStrlenFuncT)(const xmlChar*);
typedef xmlChar* (*xmlNodeGetContentFuncT)(const xmlNode*);

struct XmlParserInterface {
    xmlParseMemoryFuncT xmlParseMemoryFunc = nullptr;
    xmlDocGetRootElementFuncT xmlDocGetRootElementFunc = nullptr;
    xmlFreeDocFuncT xmlFreeDocFunc = nullptr;
    xmlCleanupParserFuncT xmlCleanupParserFunc = nullptr;
    xmlStrncmpFuncT xmlStrncmpFunc = nullptr;
    xmlStrlenFuncT xmlStrlenFunc = nullptr;
    xmlNodeGetContentFuncT xmlNodeGetContentFunc = nullptr;
};
int loc_read_conf_xml(const char* buffer, int bufLen,
        mgpOsnmaPublicKeyAndMerkleTreeStruct* merkle_tree);

static void parse_scan_elements(xmlNode *a_node,
        mgpOsnmaPublicKeyAndMerkleTreeStruct* merkle_tree, XmlParserInterface& xmlParser);
static void parse_public_key_elements(xmlNode *a_node,
        mgpOsnmaPublicKeyT& publicKey, XmlParserInterface& xmlParser);
static void parse_tree_node_elements(xmlNode *a_node,
        mgpOsnmaTreeNodeT& treeNode, XmlParserInterface& xmlParser);
static int parse_int_element(xmlNode *pXmlNode, int& value,
        const xmlChar * const xmlString, XmlParserInterface& xmlParser);
static int parse_string_element(xmlNode *pXmlNode, char* p_arr, int len,
        const xmlChar * const xmlString, XmlParserInterface& xmlParser);
static void get_node_path(int j, int i, int rootJ, int rootI, vector<int>& vec);
static mgpOsnmaTreeNodeT find_node(int j, int i, vector<mgpOsnmaTreeNodeT>& vec);

#endif // XML_FILE_PARSER_H
