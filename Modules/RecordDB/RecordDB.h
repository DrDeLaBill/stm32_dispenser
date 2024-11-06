/* Copyright © 2023 Georgy E. All rights reserved. */

#pragma once


#include <stdint.h>

#include "StorageAT.h"


#ifdef DEBUG
#   define RECORD_BEDUG (1)
#endif


class RecordDB
{
public:
	static constexpr unsigned INPUTS_CNT = 6;

    typedef enum _RecordStatus {
        RECORD_OK = 0,
        RECORD_ERROR,
        RECORD_NO_LOG
    } RecordStatus;

    typedef struct __attribute__((packed)) _Record {
    	uint32_t id;            // Record ID
    	uint32_t time;          // Record time
    	int32_t  level;         // Liquid level
    	uint16_t press;         // First pressure sensor
    	uint32_t pump_wok_time; // Log pump down time sec
    	uint32_t pump_downtime; // Log pump work sec
    	uint8_t  inputs;        // Input pins values
    } Record;

    RecordDB(uint32_t recordId);

    RecordStatus load();
    RecordStatus loadNext();
    RecordStatus save();

    void setRecordId(uint32_t recordId);

    Record record = {};

private:
    static const char* RECORD_PREFIX;
    static const char* TAG;

    static const uint32_t CLUST_MAGIC   = 0xBEDAC0DE;
    static const uint8_t  CLUST_VERSION = 0x01;
    static const uint32_t CLUST_SIZE    = (
		(
			STORAGE_PAGE_PAYLOAD_SIZE -
			sizeof(CLUST_MAGIC) -
			sizeof(CLUST_VERSION)
		) /
		sizeof(struct _Record)
	);

    typedef struct __attribute__((packed)) _RecordClust {
        uint32_t rcrd_magic;
        uint8_t  rcrd_ver;
        Record   records[CLUST_SIZE];
    } RecordClust;


    uint32_t m_recordId;

    uint32_t m_clustId;
    RecordClust m_clust;


    RecordDB() {}

    RecordStatus loadClust(uint32_t address);
    RecordStatus getNewId(uint32_t *newId);
};
