/* Copyright © 2023 Georgy E. All rights reserved. */

#pragma once


#include <stdint.h>

#include "StorageAT.h"


#ifdef DEBUG
#   define RECORD_BEDUG (0)
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

    typedef struct __attribute__((packed)) _record_v3_t {
    	uint32_t id;             // Record ID
    	uint64_t time;           // Record time
    	int32_t  level;          // Liquid level
    	uint16_t press;          // Pressure sensor
    	uint16_t tempr;          // Temperature sensor
    	uint32_t pump_work_time; // Log pump down time sec
    	uint32_t pump_downtime;  // Log pump work sec
    	uint8_t  inputs;         // Input pins values
    } record_v3_t;

    typedef struct __attribute__((packed)) _record_v2_t {
    	uint32_t id;            // Record ID
    	uint64_t time;          // Record time
    	int32_t  level;         // Liquid level
    	uint16_t press;         // First pressure sensor
    	uint32_t pump_wok_time; // Log pump down time sec
    	uint32_t pump_downtime; // Log pump work sec
    	uint8_t  inputs;        // Input pins values
    } record_v2_t;

    RecordDB(uint32_t recordId);

    RecordStatus load();
    RecordStatus loadNext();
    RecordStatus save();

    void setRecordId(uint32_t recordId);

    record_v3_t record = {};

private:
    static const char* RECORD_PREFIX;
    static const char* TAG;

    static const uint32_t CLUST_MAGIC   = 0xBEDAC0DE;
    static const uint8_t  CLUST_VERSION = 0x03;
    static const uint32_t CLUST_SIZE    = (
		(
			STORAGE_PAGE_PAYLOAD_SIZE -
			sizeof(CLUST_MAGIC) -
			sizeof(CLUST_VERSION)
		) /
		sizeof(struct _record_v3_t)
	);

    static const uint8_t  CLUST_VERSION_V2 = 0x02;
    static const uint32_t CLUST_SIZE_V2    = (
		(
			STORAGE_PAGE_PAYLOAD_SIZE -
			sizeof(CLUST_MAGIC) -
			sizeof(CLUST_VERSION)
		) /
		sizeof(struct _record_v2_t)
	);

    typedef struct __attribute__((packed)) _record_clust_v3_t {
        uint32_t    rcrd_magic;
        uint8_t     rcrd_ver;
        record_v3_t records[CLUST_SIZE];
    } record_clust_v3_t;

    typedef struct __attribute__((packed)) _record_clust_v2_t {
        uint32_t    rcrd_magic;
        uint8_t     rcrd_ver;
        record_v2_t records[CLUST_SIZE_V2];
    } record_clust_v2_t;

    uint32_t m_recordId;

    uint32_t m_clustId;
    record_clust_v3_t m_clust;


    RecordDB() {}

    RecordStatus loadClust(uint32_t address);
    RecordStatus getNewId(uint32_t *newId);
    RecordStatus recover(uint32_t address, record_clust_v3_t& clust);
};
