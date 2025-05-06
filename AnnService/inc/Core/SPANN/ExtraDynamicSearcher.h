// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef _SPTAG_SPANN_EXTRADYNAMICSEARCHER_H_
#define _SPTAG_SPANN_EXTRADYNAMICSEARCHER_H_

#include "inc/Helper/VectorSetReader.h"
#include "inc/Helper/AsyncFileReader.h"
#include "IExtraSearcher.h"
#include "ExtraStaticSearcher.h"
#include "inc/Core/Common/TruthSet.h"
#include "inc/Helper/KeyValueIO.h"
#include "inc/Core/Common/FineGrainedLock.h"
#include "PersistentBuffer.h"
#include "inc/Core/Common/PostingSizeRecord.h"
#include "ExtraSPDKController.h"
#include <chrono>
#include <map>
#include <cmath>
#include <climits>
#include <future>
#include <numeric>
#include <utility>
#include <random>
#include <tbb/concurrent_hash_map.h>

#include <xmmintrin.h>
#ifdef ROCKSDB
#include "ExtraRocksDBController.h"
#endif

// enable rocksdb io_uring
// extern "C" bool RocksDbIOUringEnable() { return true; }

namespace SPTAG::SPANN {
    template <typename ValueType>
    class ExtraDynamicSearcher : public IExtraSearcher
    {
        class MergeAsyncJob : public Helper::ThreadPool::Job
        {
        private:
            VectorIndex* m_index;
            ExtraDynamicSearcher<ValueType>* m_extraIndex;
            SizeType headID;
            bool disableReassign;
            std::function<void()> m_callback;
        public:
            MergeAsyncJob(VectorIndex* headIndex, ExtraDynamicSearcher<ValueType>* extraIndex, SizeType headID, bool disableReassign, std::function<void()> p_callback)
                : m_index(headIndex), m_extraIndex(extraIndex), headID(headID), disableReassign(disableReassign), m_callback(std::move(p_callback)) {}

            ~MergeAsyncJob() {}

            inline void exec(IAbortOperation* p_abort) override {
                m_extraIndex->MergePostings(m_index, headID, !disableReassign);
                if (m_callback != nullptr) {
                    m_callback();
                }
            }
        };

        class ConcurrentMergeAsyncJob : public Helper::ThreadPool::Job
        {
        private:
            VectorIndex* m_index;
            ExtraDynamicSearcher<ValueType>* m_extraIndex;
            SizeType headID;
            bool disableReassign;
            std::function<void()> m_callback;
        public:
            ConcurrentMergeAsyncJob(VectorIndex* headIndex, ExtraDynamicSearcher<ValueType>* extraIndex, SizeType headID, bool disableReassign, std::function<void()> p_callback)
                : m_index(headIndex), m_extraIndex(extraIndex), headID(headID), disableReassign(disableReassign), m_callback(std::move(p_callback)) {}

            ~ConcurrentMergeAsyncJob() {}

            inline void exec(IAbortOperation* p_abort) override {
                m_extraIndex->ConcurrentMergePostings(m_index, headID, !disableReassign);
                if (m_callback != nullptr) {
                    m_callback();
                }
            }
        };

        class SplitAsyncJob : public Helper::ThreadPool::Job
        {
        private:
            VectorIndex* m_index;
            ExtraDynamicSearcher<ValueType>* m_extraIndex;
            SizeType headID;
            bool disableReassign;
            std::function<void()> m_callback;
        public:
            SplitAsyncJob(VectorIndex* headIndex, ExtraDynamicSearcher<ValueType>* extraIndex, SizeType headID, bool disableReassign, std::function<void()> p_callback)
                : m_index(headIndex), m_extraIndex(extraIndex), headID(headID), disableReassign(disableReassign), m_callback(std::move(p_callback)) {}

            ~SplitAsyncJob() {}

            inline void exec(IAbortOperation* p_abort) override {
                m_extraIndex->Split(m_index, headID, !disableReassign);
                //m_extraIndex->ConcurrentSplit(m_index, headID, !disableReassign);
                if (m_callback != nullptr) {
                    m_callback();
                }
            }
        };

        class ConcurrentSplitAsyncJob : public Helper::ThreadPool::Job
        {
        private:
            VectorIndex* m_index;
            ExtraDynamicSearcher<ValueType>* m_extraIndex;
            SizeType headID;
            bool disableReassign;
            std::function<void()> m_callback;
        public:
            ConcurrentSplitAsyncJob(VectorIndex* headIndex, ExtraDynamicSearcher<ValueType>* extraIndex, SizeType headID, bool disableReassign, std::function<void()> p_callback)
                : m_index(headIndex), m_extraIndex(extraIndex), headID(headID), disableReassign(disableReassign), m_callback(std::move(p_callback)) {}

            ~ConcurrentSplitAsyncJob() {}

            inline void exec(IAbortOperation* p_abort) override {
                //m_extraIndex->Split(m_index, headID, !disableReassign);
                m_extraIndex->ConcurrentSplit(m_index, headID, !disableReassign);
                if (m_callback != nullptr) {
                    m_callback();
                }
            }
        };

        class ReassignAsyncJob : public Helper::ThreadPool::Job
        {
        private:
            VectorIndex* m_index;
            ExtraDynamicSearcher<ValueType>* m_extraIndex;
            std::shared_ptr<std::string> vectorInfo;
            SizeType HeadPrev;
            std::function<void()> m_callback;
        public:
            ReassignAsyncJob(VectorIndex* headIndex, ExtraDynamicSearcher<ValueType>* extraIndex,
                std::shared_ptr<std::string> vectorInfo, SizeType HeadPrev, std::function<void()> p_callback)
                : m_index(headIndex), m_extraIndex(extraIndex), vectorInfo(std::move(vectorInfo)), HeadPrev(HeadPrev), m_callback(std::move(p_callback)) {}

            ~ReassignAsyncJob() {}

            void exec(IAbortOperation* p_abort) override {
                m_extraIndex->Reassign(m_index, vectorInfo, HeadPrev);
                if (m_callback != nullptr) {
                    m_callback();
                }
            }
        };

        class ConcurrentReassignAsyncJob : public Helper::ThreadPool::Job
        {
        private:
            VectorIndex* m_index;
            ExtraDynamicSearcher<ValueType>* m_extraIndex;
            std::shared_ptr<std::string> vectorInfo;
            SizeType HeadPrev;
            std::function<void()> m_callback;
        public:
            ConcurrentReassignAsyncJob(VectorIndex* headIndex, ExtraDynamicSearcher<ValueType>* extraIndex,
                std::shared_ptr<std::string> vectorInfo, SizeType HeadPrev, std::function<void()> p_callback)
                : m_index(headIndex), m_extraIndex(extraIndex), vectorInfo(std::move(vectorInfo)), HeadPrev(HeadPrev), m_callback(std::move(p_callback)) {}

            ~ConcurrentReassignAsyncJob() {}

            void exec(IAbortOperation* p_abort) override {
                m_extraIndex->ConcurrentReassignSingleVector(m_index, vectorInfo, HeadPrev);
                if (m_callback != nullptr) {
                    m_callback();
                }
            }
        };

        class ConcurrentAppendAsyncJob : public Helper::ThreadPool::Job
        {
        private:
            VectorIndex* m_index;
            ExtraDynamicSearcher<ValueType>* m_extraIndex;
            std::shared_ptr<std::string> vectorInfo;
            SizeType HeadID;
            int appendNum;
            std::function<void()> m_callback;
        public:
            ConcurrentAppendAsyncJob(VectorIndex* headIndex, ExtraDynamicSearcher<ValueType>* extraIndex,
                std::shared_ptr<std::string> vectorInfo, SizeType HeadID, int appendNum, std::function<void()> p_callback)
                : m_index(headIndex), m_extraIndex(extraIndex), vectorInfo(vectorInfo), HeadID(HeadID), appendNum(appendNum), m_callback(std::move(p_callback)) {}

            ~ConcurrentAppendAsyncJob() {}

            void exec(IAbortOperation* p_abort) override {
                m_extraIndex->ConcurrentAppend(m_index, HeadID, appendNum, *(vectorInfo.get()));
                if (m_callback != nullptr) {
                    m_callback();
                }
            }
        };

        class ConcurrentDetectAsyncJob : public Helper::ThreadPool::Job
        {
        private:
            VectorIndex* m_index;
            ExtraDynamicSearcher<ValueType>* m_extraIndex;
            
            std::function<void()> m_callback;
        public:
            ConcurrentDetectAsyncJob(VectorIndex* headIndex, ExtraDynamicSearcher<ValueType>* extraIndex,
                std::function<void()> p_callback)
                : m_index(headIndex), m_extraIndex(extraIndex), m_callback(std::move(p_callback)) {}

            ~ConcurrentDetectAsyncJob() {}

            void exec(IAbortOperation* p_abort) override {
                m_extraIndex->detectPostingLength(m_index);
                if (m_callback != nullptr) {
                    m_callback();
                }
            }
        };

        class SPDKThreadPool : public Helper::ThreadPool
        {
        public:
            void initSPDK(int numberOfThreads, ExtraDynamicSearcher<ValueType>* extraIndex) 
            {
                m_abort.SetAbort(false);
                for (int i = 0; i < numberOfThreads; i++)
                {
                    m_threads.emplace_back([this, extraIndex] {
                        extraIndex->Initialize();
                        Job *j;
                        while (get(j))
                        {
                            try 
                            {
                                currentJobs++;
                                j->exec(&m_abort);
                                currentJobs--;
                            }
                            catch (std::exception& e) {
                                LOG(Helper::LogLevel::LL_Error, "ThreadPool: exception in %s %s\n", typeid(*j).name(), e.what());
                            }
                            
                            delete j;
                        }
                        extraIndex->ExitBlockController();
                    });
                }
            }
        };

    private:
        std::shared_ptr<Helper::KeyValueIO> db;

        COMMON::VersionLabel* m_versionMap;
        COMMON::PostingVersionLabel* m_postingVersionMap;
        Options* m_opt;

        std::mutex m_dataAddLock;

        std::mutex m_mergeLock;

        COMMON::FineGrainedRWLock m_rwLocks;

        COMMON::PostingSizeRecord m_postingSizes;

        std::shared_ptr<SPDKThreadPool> m_splitThreadPool;
        std::shared_ptr<SPDKThreadPool> m_reassignThreadPool;

        IndexStats m_stat;
        std::chrono::steady_clock::time_point last_record_time;
        // tbb::concurrent_hash_map<SizeType, SizeType> m_splitList;

        std::mutex m_runningLock;
        std::unordered_set<SizeType> m_splitList;

        tbb::concurrent_hash_map<SizeType, SizeType> m_mergeList;
        //statistics
        int delete_branch = 0;
        int normal_branch = 0;
        int split_merge_branch = 0;
        std::atomic<int> db_get_num;
        std::atomic<int> db_put_num;
        std::atomic<int> search_count;

    public:
        ExtraDynamicSearcher(const char* dbPath, int dim, int postingBlockLimit, bool useDirectIO, float searchLatencyHardLimit, int mergeThreshold, bool useSPDK = false, int batchSize = 64, int bufferLength = 3) {
            if (useSPDK) {
                db.reset(new SPDKIO(dbPath, 1024 * 1024, MaxSize, postingBlockLimit + bufferLength, 1024, batchSize));
                m_postingSizeLimit = postingBlockLimit * PageSize / (sizeof(ValueType) * dim + sizeof(int) + sizeof(uint8_t));
            } else {
#ifdef ROCKSDB
                db.reset(new RocksDBIO(dbPath, useDirectIO));
                m_postingSizeLimit = postingBlockLimit;
#endif
            }
            m_metaDataSize = sizeof(int) + sizeof(uint8_t);
            m_vectorInfoSize = dim * sizeof(ValueType) + m_metaDataSize;
            m_hardLatencyLimit = std::chrono::microseconds((int)searchLatencyHardLimit * 1000);
            m_mergeThreshold = mergeThreshold;
            last_record_time = std::chrono::steady_clock::now();
            LOG(Helper::LogLevel::LL_Info, "Posting size limit: %d, search limit: %f, merge threshold: %d\n", m_postingSizeLimit, searchLatencyHardLimit, m_mergeThreshold);
        }

        ~ExtraDynamicSearcher() {}

        void detectPostingLength(VectorIndex* p_index){
            
            for (int i = 0; i < p_index->GetNumSamples(); i++) {
                if (p_index->ContainSample(i)) {
                    SizeType size = m_postingSizes.GetSize(i);
                    if(size > m_postingSizeLimit){
                        if(m_postingVersionMap->IsNormal(i)){
                            ConcurrentSplitAsync(p_index, i);
                        }
                    }
                    else if(size < m_mergeThreshold){
                        if(m_postingVersionMap->IsNormal(i)){
                            ConcurrentMergeAsync(p_index, i);
                        }
                    }
                }
                
            }
        }

        inline void ConcurrentDetectAsync(VectorIndex* p_index, std::function<void()> p_callback = nullptr)
        {
            auto* curJob = new ConcurrentDetectAsyncJob(p_index, this, p_callback);
            m_splitThreadPool->add(curJob);
        }

        //headCandidates: search data structrue for "vid" vector
        //headID: the head vector that stands for vid
        bool IsAssumptionBroken(VectorIndex* p_index, SizeType headID, QueryResult& headCandidates, SizeType vid)
        {
            p_index->SearchIndex(headCandidates);
            int replicaCount = 0;
            BasicResult* queryResults = headCandidates.GetResults();
            std::vector<Edge> selections(static_cast<size_t>(m_opt->m_replicaCount));
            for (int i = 0; i < headCandidates.GetResultNum() && replicaCount < m_opt->m_replicaCount; ++i) {
                if (queryResults[i].VID == -1) {
                    break;
                }
                // RNG Check.
                bool rngAccpeted = true;
                for (int j = 0; j < replicaCount; ++j) {
                    float nnDist = p_index->ComputeDistance(
                        p_index->GetSample(queryResults[i].VID),
                        p_index->GetSample(selections[j].node));
                    if (nnDist < queryResults[i].Dist) {
                        rngAccpeted = false;
                        break;
                    }
                }
                if (!rngAccpeted)
                    continue;

                selections[replicaCount].node = queryResults[i].VID;
                // LOG(Helper::LogLevel::LL_Info, "head:%d\n", queryResults[i].VID);
                if (selections[replicaCount].node == headID) return false;
                ++replicaCount;
            }
            return true;
        }

        //Measure that in "headID" posting list, how many vectors break their assumption
        int QuantifyAssumptionBroken(VectorIndex* p_index, SizeType headID, std::string& postingList, SizeType SplitHead, std::vector<SizeType>& newHeads, std::set<int>& brokenID, int topK = 0, float ratio = 1.0)
        {
            int assumptionBrokenNum = 0;
            int postVectorNum = postingList.size() / m_vectorInfoSize;
            uint8_t* postingP = reinterpret_cast<uint8_t*>(&postingList.front());
            float minDist;
            float maxDist;
            float avgDist = 0;
            std::vector<float> distanceSet;
            //#pragma omp parallel for num_threads(32)
            for (int j = 0; j < postVectorNum; j++) {
                uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                SizeType vid = *(reinterpret_cast<int*>(vectorId));
                uint8_t version = *(reinterpret_cast<uint8_t*>(vectorId + sizeof(int)));
                float_t dist = p_index->ComputeDistance(reinterpret_cast<ValueType*>(vectorId + m_metaDataSize), p_index->GetSample(headID));
                // if (dist < Epsilon) LOG(Helper::LogLevel::LL_Info, "head found: vid: %d, head: %d\n", vid, headID);
                avgDist += dist;
                distanceSet.push_back(dist);
                if (m_versionMap->Deleted(vid) || m_versionMap->GetVersion(vid) != version) continue;
                COMMON::QueryResultSet<ValueType> headCandidates(reinterpret_cast<ValueType*>(vectorId + m_metaDataSize), 64);
                if (brokenID.find(vid) == brokenID.end() && IsAssumptionBroken(headID, headCandidates, vid)) {
                    /*
                    float_t headDist = p_index->ComputeDistance(headCandidates.GetTarget(), p_index->GetSample(SplitHead));
                    float_t newHeadDist_1 = p_index->ComputeDistance(headCandidates.GetTarget(), p_index->GetSample(newHeads[0]));
                    float_t newHeadDist_2 = p_index->ComputeDistance(headCandidates.GetTarget(), p_index->GetSample(newHeads[1]));

                    float_t splitDist = p_index->ComputeDistance(p_index->GetSample(SplitHead), p_index->GetSample(headID));

                    float_t headToNewHeadDist_1 = p_index->ComputeDistance(p_index->GetSample(headID), p_index->GetSample(newHeads[0]));
                    float_t headToNewHeadDist_2 = p_index->ComputeDistance(p_index->GetSample(headID), p_index->GetSample(newHeads[1]));

                    LOG(Helper::LogLevel::LL_Info, "broken vid to head distance: %f, to split head distance: %f\n", dist, headDist);
                    LOG(Helper::LogLevel::LL_Info, "broken vid to new head 1 distance: %f, to new head 2 distance: %f\n", newHeadDist_1, newHeadDist_2);
                    LOG(Helper::LogLevel::LL_Info, "head to spilit head distance: %f\n", splitDist);
                    LOG(Helper::LogLevel::LL_Info, "head to new head 1 distance: %f, to new head 2 distance: %f\n", headToNewHeadDist_1, headToNewHeadDist_2);
                    */
                    assumptionBrokenNum++;
                    brokenID.insert(vid);
                }
            }

            if (assumptionBrokenNum != 0) {
                std::sort(distanceSet.begin(), distanceSet.end());
                minDist = distanceSet[1];
                maxDist = distanceSet.back();
                // LOG(Helper::LogLevel::LL_Info, "distance: min: %f, max: %f, avg: %f, 50th: %f\n", minDist, maxDist, avgDist/postVectorNum, distanceSet[distanceSet.size() * 0.5]);
                // LOG(Helper::LogLevel::LL_Info, "assumption broken num: %d\n", assumptionBrokenNum);
                float_t splitDist = p_index->ComputeDistance(p_index->GetSample(SplitHead), p_index->GetSample(headID));

                float_t headToNewHeadDist_1 = p_index->ComputeDistance(p_index->GetSample(headID), p_index->GetSample(newHeads[0]));
                float_t headToNewHeadDist_2 = p_index->ComputeDistance(p_index->GetSample(headID), p_index->GetSample(newHeads[1]));

                // LOG(Helper::LogLevel::LL_Info, "head to spilt head distance: %f/%d/%.2f\n", splitDist, topK, ratio);
                // LOG(Helper::LogLevel::LL_Info, "head to new head 1 distance: %f, to new head 2 distance: %f\n", headToNewHeadDist_1, headToNewHeadDist_2);
            }

            return assumptionBrokenNum;
        }

        int QuantifySplitCaseA(std::vector<SizeType>& newHeads, std::vector<std::string>& postingLists, SizeType SplitHead, int split_order, std::set<int>& brokenID)
        {
            int assumptionBrokenNum = 0;
            assumptionBrokenNum += QuantifyAssumptionBroken(newHeads[0], postingLists[0], SplitHead, newHeads, brokenID);
            assumptionBrokenNum += QuantifyAssumptionBroken(newHeads[1], postingLists[1], SplitHead, newHeads, brokenID);
            int vectorNum = (postingLists[0].size() + postingLists[1].size()) / m_vectorInfoSize;
            LOG(Helper::LogLevel::LL_Info, "After Split%d, Top0 nearby posting lists, caseA : %d/%d\n", split_order, assumptionBrokenNum, vectorNum);
            return assumptionBrokenNum;
        }

        //Measure that around "headID", how many vectors break their assumption
        //"headID" is the head vector before split
        void QuantifySplitCaseB(VectorIndex* p_index, SizeType headID, std::vector<SizeType>& newHeads, SizeType SplitHead, int split_order, int assumptionBrokenNum_top0, std::set<int>& brokenID)
        {
            COMMON::QueryResultSet<ValueType> nearbyHeads(reinterpret_cast<const ValueType*>(p_index->GetSample(headID)), 64);
            std::vector<std::string> postingLists;
            p_index->SearchIndex(nearbyHeads);
            std::string postingList;
            BasicResult* queryResults = nearbyHeads.GetResults();
            int topk = 8;
            int assumptionBrokenNum = assumptionBrokenNum_top0;
            int assumptionBrokenNum_topK = assumptionBrokenNum_top0;
            int i;
            int containedHead = 0;
            if (assumptionBrokenNum_top0 != 0) containedHead++;
            int vectorNum = 0;
            float furthestDist = 0;
            for (i = 0; i < nearbyHeads.GetResultNum(); i++) {
                if (queryResults[i].VID == -1) {
                    break;
                }
                furthestDist = queryResults[i].Dist;
                if (i == topk) {
                    LOG(Helper::LogLevel::LL_Info, "After Split%d, Top%d nearby posting lists, caseB : %d in %d/%d\n", split_order, i, assumptionBrokenNum, containedHead, vectorNum);
                    topk *= 2;
                }
                if (queryResults[i].VID == newHeads[0] || queryResults[i].VID == newHeads[1]) continue;
                db->Get(queryResults[i].VID, &postingList);
                vectorNum += postingList.size() / m_vectorInfoSize;
                int tempNum = QuantifyAssumptionBroken(queryResults[i].VID, postingList, SplitHead, newHeads, brokenID, i, queryResults[i].Dist / queryResults[1].Dist);
                assumptionBrokenNum += tempNum;
                if (tempNum != 0) containedHead++;
            }
            LOG(Helper::LogLevel::LL_Info, "After Split%d, Top%d nearby posting lists, caseB : %d in %d/%d\n", split_order, i, assumptionBrokenNum, containedHead, vectorNum);
        }

        void QuantifySplit(SizeType headID, std::vector<std::string>& postingLists, std::vector<SizeType>& newHeads, SizeType SplitHead, int split_order)
        {
            std::set<int> brokenID;
            brokenID.clear();
            // LOG(Helper::LogLevel::LL_Info, "Split Quantify: %d, head1:%d, head2:%d\n", split_order, newHeads[0], newHeads[1]);
            int assumptionBrokenNum = QuantifySplitCaseA(newHeads, postingLists, SplitHead, split_order, brokenID);
            QuantifySplitCaseB(headID, newHeads, SplitHead, split_order, assumptionBrokenNum, brokenID);
        }

        bool CheckIsNeedReassign(VectorIndex* p_index, std::vector<SizeType>& newHeads, ValueType* data, SizeType splitHead, float_t headToSplitHeadDist, float_t currentHeadDist, bool isInSplitHead, SizeType currentHead)
        {

            float_t splitHeadDist = p_index->ComputeDistance(data, p_index->GetSample(splitHead));

            if (isInSplitHead) {
                if (splitHeadDist >= currentHeadDist) return false;
            }
            else {
                float_t newHeadDist_1 = p_index->ComputeDistance(data, p_index->GetSample(newHeads[0]));
                float_t newHeadDist_2 = p_index->ComputeDistance(data, p_index->GetSample(newHeads[1]));
                if (splitHeadDist <= newHeadDist_1 && splitHeadDist <= newHeadDist_2) return false;
                if (currentHeadDist <= newHeadDist_1 && currentHeadDist <= newHeadDist_2) return false;
            }
            return true;
        }

        inline void Serialize(char* ptr, SizeType VID, std::uint8_t version, const void* vector) {
            memcpy(ptr, &VID, sizeof(VID));
            memcpy(ptr + sizeof(VID), &version, sizeof(version));
            memcpy(ptr + m_metaDataSize, vector, m_vectorInfoSize - m_metaDataSize);
        }

        void CalculatePostingDistribution(VectorIndex* p_index)
        {
            if (m_opt->m_inPlace) return;
            int top = m_postingSizeLimit / 10 + 1;
            int page = m_opt->m_postingPageLimit + 1;
            std::vector<int> lengthDistribution(top, 0);
            std::vector<int> sizeDistribution(page + 2, 0);
            int deletedHead = 0;
            for (int i = 0; i < p_index->GetNumSamples(); i++) {
                if (!p_index->ContainSample(i)) deletedHead++;
                lengthDistribution[m_postingSizes.GetSize(i) / 10]++;
                int size = m_postingSizes.GetSize(i) * m_vectorInfoSize;
                if (size < PageSize) {
                    if (size < 512) sizeDistribution[0]++;
                    else if (size < 1024) sizeDistribution[1]++;
                    else sizeDistribution[2]++;
                }
                else {
                    sizeDistribution[size / PageSize + 2]++;
                }
            }
            LOG(Helper::LogLevel::LL_Info, "Posting Length (Vector Num):\n");
            for (int i = 0; i < top; ++i)
            {
                LOG(Helper::LogLevel::LL_Info, "%d ~ %d: %d, \n", i * 10, (i + 1) * 10 - 1, lengthDistribution[i]);
            }
            LOG(Helper::LogLevel::LL_Info, "Posting Length (Data Size):\n");
            for (int i = 0; i < page + 2; ++i)
            {
                if (i <= 2) {
                    if (i == 0) LOG(Helper::LogLevel::LL_Info, "0 ~ 512 B: %d, \n", sizeDistribution[0] - deletedHead);
                    else if (i == 1) LOG(Helper::LogLevel::LL_Info, "512 B ~ 1 KB: %d, \n", sizeDistribution[1]);
                    else LOG(Helper::LogLevel::LL_Info, "1 KB ~ 4 KB: %d, \n", sizeDistribution[2]);
                }
                else
                    LOG(Helper::LogLevel::LL_Info, "%d ~ %d KB: %d, \n", (i - 2) * 4, (i - 1) * 4, sizeDistribution[i]);
            }
        }

        // TODO
        void RefineIndex(std::shared_ptr<Helper::VectorSetReader>& p_reader, std::shared_ptr<VectorIndex> p_index)
        {
            LOG(Helper::LogLevel::LL_Info, "Begin PreReassign\n");
            std::atomic_bool doneReassign = false;
            // p_index->UpdateIndex();
            LOG(Helper::LogLevel::LL_Info, "Into PreReassign Loop\n");
            while (!doneReassign) {
                auto preReassignTimeBegin = std::chrono::high_resolution_clock::now();
                doneReassign = true;
                std::vector<std::thread> threads;
                std::atomic_int nextPostingID(0);
                int currentPostingNum = p_index->GetNumSamples();
                int limit = m_postingSizeLimit * m_opt->m_preReassignRatio;
                LOG(Helper::LogLevel::LL_Info, "Batch PreReassign, Current PostingNum: %d, Current Limit: %d\n", currentPostingNum, limit);
                auto func = [&]()
                {
                    int index = 0;
                    Initialize();
                    while (true)
                    {
                        index = nextPostingID.fetch_add(1);
                        if (index < currentPostingNum)
                        {
                            if ((index & ((1 << 14) - 1)) == 0)
                            {
                                LOG(Helper::LogLevel::LL_Info, "Sent %.2lf%%...\n", index * 100.0 / currentPostingNum);
                            }
                            if (m_postingSizes.GetSize(index) >= limit)
                            {
                                doneReassign = false;
                                //Split(p_index.get(), index, false, true);
                                ConcurrentSplit(p_index.get(), index, false, true);
                            }
                        }
                        else
                        {
                            ExitBlockController();
                            return;
                        }
                    }
                };
                for (int j = 0; j < m_opt->m_iSSDNumberOfThreads; j++) { threads.emplace_back(func); }
                for (auto& thread : threads) { thread.join(); }
                auto preReassignTimeEnd = std::chrono::high_resolution_clock::now();
                double elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(preReassignTimeEnd - preReassignTimeBegin).count();
                LOG(Helper::LogLevel::LL_Info, "rebuild cost: %.2lf s\n", elapsedSeconds);

                //p_index->SaveIndex(m_opt->m_indexDirectory + FolderSep + m_opt->m_headIndexFolder);
                //LOG(Helper::LogLevel::LL_Info, "SPFresh: ReWriting SSD Info\n");
                //m_postingSizes.Save(m_opt->m_ssdInfoFile);

                // for (int i = 0; i < p_index->GetNumSamples(); i++) {
                //     db->Delete(i);
                // }
                // ForceCompaction();
                p_index->SaveIndex(m_opt->m_indexDirectory + FolderSep + m_opt->m_headIndexFolder);
                BuildIndex(p_reader, p_index, *m_opt, *m_versionMap, *m_postingVersionMap);
                // ForceCompaction();
                CalculatePostingDistribution(p_index.get());

                // p_index->SaveIndex(m_opt->m_indexDirectory + FolderSep + m_opt->m_headIndexFolder);
                LOG(Helper::LogLevel::LL_Info, "SPFresh: ReWriting SSD Info\n");
                m_postingSizes.Save(m_opt->m_ssdInfoFile);
            }
        }

        void RunningLockAndErase(SizeType headID){
            std::lock_guard<std::mutex> tmplock(m_runningLock);
            // LOG(Helper::LogLevel::LL_Info,"erase: %d\n", headID);
            if(m_splitList.find(headID)!=m_splitList.end())
                m_splitList.erase(headID);
        }

        ErrorCode Split(VectorIndex* p_index, const SizeType headID, bool reassign = false, bool preReassign = false)
        {
            auto splitBegin = std::chrono::high_resolution_clock::now();
            // LOG(Helper::LogLevel::LL_Info, "into split: %d\n", headID);
            std::vector<SizeType> newHeadsID;
            std::vector<std::string> newPostingLists;
            double elapsedMSeconds;
            {
                std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);//lock posting headID

                std::string postingList;
                auto splitGetBegin = std::chrono::high_resolution_clock::now();
                if (db->Get(headID, &postingList) != ErrorCode::Success) {//read posting data from ssd storage, data is stored in postingList
                    LOG(Helper::LogLevel::LL_Info, "Split fail to get oversized postings\n");
                    exit(0);
                }
                auto splitGetEnd = std::chrono::high_resolution_clock::now();
                elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(splitGetEnd - splitGetBegin).count();
                m_stat.m_getCost += elapsedMSeconds;
                // reinterpret postingList to vectors and IDs
                auto* postingP = reinterpret_cast<uint8_t*>(&postingList.front());//get the first character's address of postingList
                SizeType postVectorNum = (SizeType)(postingList.size() / m_vectorInfoSize);//get the vector num in posting headID
               
                COMMON::Dataset<ValueType> smallSample(postVectorNum, m_opt->m_dim, p_index->m_iDataBlockSize, p_index->m_iDataCapacity, (const void*)postingP, true, nullptr, m_metaDataSize, m_vectorInfoSize);
                //COMMON::Dataset<ValueType> smallSample(0, m_opt->m_dim, p_index->m_iDataBlockSize, p_index->m_iDataCapacity);  // smallSample[i] -> VID
                //std::vector<int> localIndicesInsert(postVectorNum);  // smallSample[i] = j <-> localindices[j] = i
                //std::vector<uint8_t> localIndicesInsertVersion(postVectorNum);
                std::vector<int> localIndices(postVectorNum);
                int index = 0;
                uint8_t* vectorId = postingP;
                for (int j = 0; j < postVectorNum; j++, vectorId += m_vectorInfoSize)//the loop is to get the all vectors of the target posting that not be deleted or updated(holds a lower version value)
                {
                    //Vector ID(SizeType, 4 bytes), then version(uint8_t, 1 byte), then Vector(raw vector data, vector.dim*sizeof(vectorType))
                    //LOG(Helper::LogLevel::LL_Info, "vector index/total:id: %d/%d:%d\n", j, m_postingSizes.GetSize(headID), *((int*)vectorId));
                    uint8_t version = *(vectorId + sizeof(SizeType));
                    int VID = *((int*)(vectorId));
                    if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) continue;

                    //localIndicesInsert[index] = VID;
                    //localIndicesInsertVersion[index] = version;
                    //smallSample.AddBatch(1, (ValueType*)(vectorId + m_metaDataSize));
                    localIndices[index] = j;
                    index++;
                }
                // double gcEndTime = sw.getElapsedMs();
                // m_splitGcCost += gcEndTime;
                if (m_opt->m_inPlace || (!preReassign && index < m_postingSizeLimit))
                {
                    char* ptr = (char*)(postingList.c_str());
                    for (int j = 0; j < index; j++, ptr += m_vectorInfoSize)
                    {
                        if (j == localIndices[j]) continue;
                        memcpy(ptr, postingList.c_str() + localIndices[j] * m_vectorInfoSize, m_vectorInfoSize);
                        //Serialize(ptr, localIndicesInsert[j], localIndicesInsertVersion[j], smallSample[j]);
                    }
                    postingList.resize(index * m_vectorInfoSize);
                    m_postingSizes.UpdateSize(headID, index);
                    if (db->Put(headID, postingList) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Info, "Split Fail to write back postings\n");
                        exit(0);
                    }
                    m_stat.m_garbageNum++;
                    auto GCEnd = std::chrono::high_resolution_clock::now();
                    elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(GCEnd - splitBegin).count();
                    m_stat.m_garbageCost += elapsedMSeconds;
                    {
                        RunningLockAndErase(headID);
                        /*std::lock_guard<std::mutex> tmplock(m_runningLock);
                        // LOG(Helper::LogLevel::LL_Info,"erase: %d\n", headID);
                        m_splitList.erase(headID);*/
                    }
                    // LOG(Helper::LogLevel::LL_Info, "GC triggered: %d, new length: %d\n", headID, index);
                    return ErrorCode::Success;
                }
                //LOG(Helper::LogLevel::LL_Info, "Resize\n");
                localIndices.resize(index);

                auto clusterBegin = std::chrono::high_resolution_clock::now();
                // k = 2, maybe we can change the split number, now it is fixed
                SPTAG::COMMON::KmeansArgs<ValueType> args(2, smallSample.C(), (SizeType)localIndices.size(), 1, p_index->GetDistCalcMethod());
                std::shuffle(localIndices.begin(), localIndices.end(), std::mt19937(std::random_device()()));

                int numClusters = SPTAG::COMMON::KmeansClustering(smallSample, localIndices, 0, (SizeType)localIndices.size(), args, 1000, 100.0F, false, nullptr, m_opt->m_virtualHead);

                auto clusterEnd = std::chrono::high_resolution_clock::now();
                elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(clusterEnd - clusterBegin).count();
                m_stat.m_clusteringCost += elapsedMSeconds;
                // int numClusters = ClusteringSPFresh(smallSample, localIndices, 0, localIndices.size(), args, 10, false, m_opt->m_virtualHead);
                // exit(0);
                if (numClusters <= 1)
                {
                    LOG(Helper::LogLevel::LL_Info, "Cluserting Failed (The same vector), Only Keep one\n");
                    std::string newpostingList(1 * m_vectorInfoSize, '\0');
                    char* ptr = (char*)(newpostingList.c_str());
                    for (int j = 0; j < 1; j++, ptr += m_vectorInfoSize)
                    {
                        memcpy(ptr, postingList.c_str() + localIndices[j] * m_vectorInfoSize, m_vectorInfoSize);
                        //Serialize(ptr, localIndicesInsert[j], localIndicesInsertVersion[j], smallSample[j]);
                    }
                    m_postingSizes.UpdateSize(headID, 1);
                    if (db->Put(headID, newpostingList) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Info, "Split fail to override postings cut to limit\n");
                        exit(0);
                    }
                    {
                        RunningLockAndErase(headID);
                        /*std::lock_guard<std::mutex> tmplock(m_runningLock);
                        m_splitList.erase(headID);*/
                    }
                    return ErrorCode::Success;
                }

                long long newHeadVID = -1;
                int first = 0;
                bool theSameHead = false;
                newPostingLists.resize(2);

                float dis1 = p_index->ComputeDistance(args.centers, p_index->GetSample(headID));
                float dis2 = p_index->ComputeDistance(args.centers + args._D, p_index->GetSample(headID));
                if(dis1 < Epsilon || dis2 < Epsilon) {
                    if(std::min(args.counts[0],args.counts[1]) / localIndices.size() <= 0.1){
                        //LOG(Helper::LogLevel::LL_Info, "Same head\n");

                        int b = args.counts[0] < args.counts[1] ? 0 : 1;
                        int start_pos = (b == 0) ? 0 : args.counts[0];
                        
                        auto* postingP = reinterpret_cast<uint8_t*>(&postingList.front());
                        for (int j = 0; j < args.counts[b]; j++)
                        {
                            //memcpy(ptr, postingList.c_str() + localIndices[start_pos + j] * m_vectorInfoSize, m_vectorInfoSize);
                            uint8_t* vectorId = postingP + localIndices[start_pos + j] * m_vectorInfoSize;
                            ReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), headID);
                        }
                        m_stat.m_theSameHeadNum++;
                        return ErrorCode::Success; 
                    }
                }

                for (int k = 0; k < 2; k++) {
                    if (args.counts[k] == 0)	continue;//arg.counts[k] is the vector num of centroid (args.centers + k * args._D)
                    
                    newPostingLists[k].resize(args.counts[k] * m_vectorInfoSize);
                    char* ptr = (char*)(newPostingLists[k].c_str());
                    for (int j = 0; j < args.counts[k]; j++, ptr += m_vectorInfoSize)//[first+j, first+ args.counts[k]) is the vector offset of new posting k
                    {
                        memcpy(ptr, postingList.c_str() + localIndices[first + j] * m_vectorInfoSize, m_vectorInfoSize);
                        //Serialize(ptr, localIndicesInsert[localIndices[first + j]], localIndicesInsertVersion[localIndices[first + j]], smallSample[localIndices[first + j]]);
                    }
                    //args.centers is the address of two new centroids
                    //if the new centroid is too closer to the original centroid headID, apply the original centroid headID
                    if (!theSameHead && p_index->ComputeDistance(args.centers + k * args._D, p_index->GetSample(headID)) < Epsilon) {
                        newHeadsID.push_back(headID);
                        newHeadVID = headID;
                        theSameHead = true;
                        auto splitPutBegin = std::chrono::high_resolution_clock::now();
                        if (!preReassign && db->Put(newHeadVID, newPostingLists[k]) != ErrorCode::Success) {
                            LOG(Helper::LogLevel::LL_Info, "Fail to override postings\n");
                            exit(0);
                        }
                        auto splitPutEnd = std::chrono::high_resolution_clock::now();
                        elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(splitPutEnd - splitPutBegin).count();
                        m_stat.m_putCost += elapsedMSeconds;
                        m_stat.m_theSameHeadNum++;
                    }
                    else {//the new centroid is different from the original one
                        int begin, end = 0;
                        p_index->AddIndexId(args.centers + k * args._D, 1, m_opt->m_dim, begin, end);//add new centroid to the index
                        newHeadVID = begin;//the new centroid's ids
                        newHeadsID.push_back(begin);
                        //std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[newHeadVID]);
                        auto splitPutBegin = std::chrono::high_resolution_clock::now();
                        if (!preReassign && db->Put(newHeadVID, newPostingLists[k]) != ErrorCode::Success) {
                            LOG(Helper::LogLevel::LL_Info, "Fail to add new postings\n");
                            exit(0);
                        }
                        auto splitPutEnd = std::chrono::high_resolution_clock::now();
                        elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(splitPutEnd - splitPutBegin).count();
                        m_stat.m_putCost += elapsedMSeconds;
                        auto updateHeadBegin = std::chrono::high_resolution_clock::now();
                        p_index->AddIndexIdx(begin, end);//refine graph: do the operation (end-begin) time(s)
                        auto updateHeadEnd = std::chrono::high_resolution_clock::now();
                        elapsedMSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(updateHeadEnd - updateHeadBegin).count();
                        m_stat.m_updateHeadCost += elapsedMSeconds;

                        std::lock_guard<std::mutex> tmplock(m_dataAddLock);
                        if (m_postingSizes.AddBatch(1) == ErrorCode::MemoryOverFlow) {
                            LOG(Helper::LogLevel::LL_Info, "MemoryOverFlow: NnewHeadVID: %d, Map Size:%d\n", newHeadVID, m_postingSizes.BufferSize());
                            exit(1);
                        }
                    }
                    //LOG(Helper::LogLevel::LL_Info, "Head id: %d split into : %d, length: %d\n", headID, newHeadVID, args.counts[k]);
                    first += args.counts[k];
                    m_postingSizes.UpdateSize(newHeadVID, args.counts[k]);
                }
                if (!theSameHead) {
                    //new codes. add the old centroid point to the closer new posting
                    /*float dis1 = p_index->ComputeDistance(args.centers, p_index->GetSample(headID));
                    float dis2 = p_index->ComputeDistance(args.centers + args._D, p_index->GetSample(headID));

                    int target_posting_id = dis1 > dis2 ? 1 : 0;
                    long long targetHeadID = newHeadsID.at(target_posting_id);
                    std::string targetPostingList;
                    if (db->Get(targetHeadID, &targetPostingList) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Info, "Fail to get postings: %d\n", targetHeadID);
                        exit(0);
                    }
                    uint8_t version = m_versionMap->GetVersion(headID);
                    m_versionMap->IncVersion(headID, &version);
                    
                    std::string appendPosting(m_vectorInfoSize, '\0');
                    Serialize((char*)(appendPosting.c_str()), headID, version, p_index->GetSample(headID));

                    if (db->Put(targetHeadID, targetPostingList.append(appendPosting)) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Info, "Fail to get postings: %d\n", targetHeadID);
                        exit(0);
                    }*/

                    p_index->DeleteIndex(headID);
                    m_postingSizes.UpdateSize(headID, 0);
                }
            }
            {
                RunningLockAndErase(headID);
                /*std::lock_guard<std::mutex> tmplock(m_runningLock);
                // LOG(Helper::LogLevel::LL_Info,"erase: %d\n", headID);
                m_splitList.erase(headID);*/
            }
            m_stat.m_splitNum++;
            if (reassign) {
                auto reassignScanBegin = std::chrono::high_resolution_clock::now();

                CollectReAssign(p_index, headID, newPostingLists, newHeadsID);

                auto reassignScanEnd = std::chrono::high_resolution_clock::now();
                elapsedMSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(reassignScanEnd - reassignScanBegin).count();

                m_stat.m_reassignScanCost += elapsedMSeconds;
            }
            auto splitEnd = std::chrono::high_resolution_clock::now();
            elapsedMSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(splitEnd - splitBegin).count();
            m_stat.m_splitCost += elapsedMSeconds;
            return ErrorCode::Success;
        }

        void SetPostingVersionDefault(VectorIndex* p_index, SizeType headID){
            //m_postingVersionMap->ClearCurrentCount(headID);
            uint64_t new_count;
            std::string vector_cache_info = m_postingVersionMap->eraseVectorCacheInfo(headID, &new_count, m_vectorInfoSize);
            if(vector_cache_info != ""){
                int vec_num = vector_cache_info.size() / m_vectorInfoSize;
                //ConcurrentAppend(p_index, headID, vec_num, vector_cache_info);
                ConcurrentAppendAsync(p_index, std::make_shared<std::string>(reinterpret_cast<char*>(&vector_cache_info.front()), vec_num * m_vectorInfoSize), headID, vec_num);
            }

            m_postingVersionMap->SetNormal(headID);
        }

        inline std::string collect(VectorIndex* p_index, SizeType headID){
            
            uint64_t new_count;
            std::string tmp_vectors = m_postingVersionMap->eraseVectorCacheInfo(headID, &new_count, m_vectorInfoSize);
    
            return tmp_vectors;

            /*
            //uint64_t newCount;
            bool numGreater = false;
            
            std::string currentVectors, leftVectors;

            if(num >= oldCount) {
                numGreater = true;
                currentVectors.assign(oldCount * m_vectorInfoSize, '\0');
                leftVectors.assign((num - oldCount) * m_vectorInfoSize, '\0');
            }
            else{
                currentVectors.assign(num * m_vectorInfoSize, '\0');
            }

            auto* vectorsP = reinterpret_cast<uint8_t*>(&tmp_vectors.front());
            char* ptr = (char*)(currentVectors.c_str());
            if(numGreater){                   
                
                memcpy(ptr, vectorsP, oldCount * m_vectorInfoSize);
                uint8_t* leftVectorsP = vectorsP + oldCount * m_vectorInfoSize;
                char* optr = (char*)(leftVectors.c_str());
                memcpy(optr, leftVectorsP, (num - oldCount) * m_vectorInfoSize);
                
                //ShowVectorTotalInfo(leftVectors);
                uint64_t new_count = postingVersionMap->addToVectorCache(headID, num - oldCount, leftVectors);
                //m_postingVersionMap->IncCount(headID, &o_count, num - oldCount);
            }
            else{
                memcpy(ptr, vectorsP, num * m_vectorInfoSize);
                //ShowVectorTotalInfo(currentVectors);                   
            }

            vectors.append(currentVectors);
            //m_postingVersionMap->DecCount(headID, &newCount, oldCount);
            //}
            
            return vectors;*/
        }

        void ShowVectorTotalInfo(std::string vec_infos){
            auto* addr = reinterpret_cast<uint8_t*>(&vec_infos.front());
            int vec_num = vec_infos.size() / m_vectorInfoSize;

            for(int i = 0; i < vec_num; i++){
                uint8_t* sub_addr = addr + i * m_vectorInfoSize;

                uint8_t version = *(sub_addr + sizeof(SizeType));
                int VID = *((int*)(sub_addr));
                ValueType* vector_raw_data = reinterpret_cast<ValueType*>(sub_addr + m_metaDataSize);

                LOG(Helper::LogLevel::LL_Info, "VID: %d, version:%d\n", VID, version);
            }
            
        }

        ErrorCode ConcurrentSplit(VectorIndex* p_index, const SizeType headID, bool reassign = false, bool preReassign = false){
            auto splitBegin = std::chrono::high_resolution_clock::now();
            // LOG(Helper::LogLevel::LL_Info, "into split: %d\n", headID);
            std::vector<SizeType> newHeadsID;
            std::vector<std::string> newPostingLists;
            double elapsedMSeconds;
            {   
                if(!m_opt->m_enableFinedLock){
                    std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);//lock posting headID(ablation)
                }
                //m_postingVersionMap->Split(headID);//marked as splitting

                std::string postingList;
                auto splitGetBegin = std::chrono::high_resolution_clock::now();
                if (db->Get(headID, &postingList) != ErrorCode::Success) {//read posting data from ssd storage, data is stored in postingList
                    LOG(Helper::LogLevel::LL_Info, "Split fail to get oversized postings\n");
                    exit(0);
                }
                db_get_num.fetch_add(1);

                auto splitGetEnd = std::chrono::high_resolution_clock::now();
                elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(splitGetEnd - splitGetBegin).count();
                m_stat.m_getCost += elapsedMSeconds;
                // reinterpret postingList to vectors and IDs
                auto* postingP = reinterpret_cast<uint8_t*>(&postingList.front());//get the first character's address of postingList
                SizeType postVectorNum = (SizeType)(postingList.size() / m_vectorInfoSize);//get the vector num in posting headID
               
                COMMON::Dataset<ValueType> smallSample(postVectorNum, m_opt->m_dim, p_index->m_iDataBlockSize, p_index->m_iDataCapacity, (const void*)postingP, true, nullptr, m_metaDataSize, m_vectorInfoSize);
                //COMMON::Dataset<ValueType> smallSample(0, m_opt->m_dim, p_index->m_iDataBlockSize, p_index->m_iDataCapacity);  // smallSample[i] -> VID
                //std::vector<int> localIndicesInsert(postVectorNum);  // smallSample[i] = j <-> localindices[j] = i
                //std::vector<uint8_t> localIndicesInsertVersion(postVectorNum);
                std::vector<int> localIndices(postVectorNum);
                int index = 0;
                uint8_t* vectorId = postingP;
                for (int j = 0; j < postVectorNum; j++, vectorId += m_vectorInfoSize)//the loop is to get the all vectors of the target posting that not be deleted or updated(holds a lower version value)
                {
                    //Vector ID(SizeType, 4 bytes), then version(uint8_t, 1 byte), then Vector(raw vector data, vector.dim*sizeof(vectorType))
                    //LOG(Helper::LogLevel::LL_Info, "vector index/total:id: %d/%d:%d\n", j, m_postingSizes.GetSize(headID), *((int*)vectorId));
                    uint8_t version = *(vectorId + sizeof(SizeType));
                    int VID = *((int*)(vectorId));
                    if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) continue;

                    //localIndicesInsert[index] = VID;
                    //localIndicesInsertVersion[index] = version;
                    //smallSample.AddBatch(1, (ValueType*)(vectorId + m_metaDataSize));
                    localIndices[index] = j;
                    index++;
                }
                // double gcEndTime = sw.getElapsedMs();
                // m_splitGcCost += gcEndTime;
                if (m_opt->m_inPlace || (!preReassign && index < m_postingSizeLimit))
                {   
                    if(m_opt->m_enableFinedLock){
                        std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);//ablation
                    }

                    char* ptr = (char*)(postingList.c_str());
                    for (int j = 0; j < index; j++, ptr += m_vectorInfoSize)
                    {
                        if (j == localIndices[j]) continue;
                        memcpy(ptr, postingList.c_str() + localIndices[j] * m_vectorInfoSize, m_vectorInfoSize);
                        //Serialize(ptr, localIndicesInsert[j], localIndicesInsertVersion[j], smallSample[j]);
                    }
                    postingList.resize(index * m_vectorInfoSize);
                    m_postingSizes.UpdateSize(headID, index);
                    if (db->Put(headID, postingList) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Info, "Split Fail to write back postings\n");
                        exit(0);
                    }
                    db_put_num.fetch_add(1);
                    m_stat.m_garbageNum++;
                    auto GCEnd = std::chrono::high_resolution_clock::now();
                    elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(GCEnd - splitBegin).count();
                    m_stat.m_garbageCost += elapsedMSeconds;
                    {
                        RunningLockAndErase(headID);
                        /*std::lock_guard<std::mutex> tmplock(m_runningLock);
                        // LOG(Helper::LogLevel::LL_Info,"erase: %d\n", headID);
                        m_splitList.erase(headID);*/

                        SetPostingVersionDefault(p_index, headID);
                    }
                    // LOG(Helper::LogLevel::LL_Info, "GC triggered: %d, new length: %d\n", headID, index);
                    return ErrorCode::Success;
                }
                //LOG(Helper::LogLevel::LL_Info, "Resize\n");
                localIndices.resize(index);

                auto clusterBegin = std::chrono::high_resolution_clock::now();
                // k = 2, maybe we can change the split number, now it is fixed
                SPTAG::COMMON::KmeansArgs<ValueType> args(2, smallSample.C(), (SizeType)localIndices.size(), 1, p_index->GetDistCalcMethod());
                std::shuffle(localIndices.begin(), localIndices.end(), std::mt19937(std::random_device()()));

                int numClusters = SPTAG::COMMON::KmeansClustering(smallSample, localIndices, 0, (SizeType)localIndices.size(), args, 1000, 100.0F, false, nullptr, m_opt->m_virtualHead);

                auto clusterEnd = std::chrono::high_resolution_clock::now();
                elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(clusterEnd - clusterBegin).count();
                m_stat.m_clusteringCost += elapsedMSeconds;
                // int numClusters = ClusteringSPFresh(smallSample, localIndices, 0, localIndices.size(), args, 10, false, m_opt->m_virtualHead);
                // exit(0);
                if (numClusters <= 1)
                {
                    LOG(Helper::LogLevel::LL_Info, "Cluserting Failed (The same vector), Only Keep one\n");
                    if(m_opt->m_enableFinedLock){
                        std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);//ablation
                    }
                    std::string newpostingList(1 * m_vectorInfoSize, '\0');
                    char* ptr = (char*)(newpostingList.c_str());
                    for (int j = 0; j < 1; j++, ptr += m_vectorInfoSize)
                    {
                        memcpy(ptr, postingList.c_str() + localIndices[j] * m_vectorInfoSize, m_vectorInfoSize);
                        //Serialize(ptr, localIndicesInsert[j], localIndicesInsertVersion[j], smallSample[j]);
                    }
                    m_postingSizes.UpdateSize(headID, 1);
                    if (db->Put(headID, newpostingList) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Info, "Split fail to override postings cut to limit\n");
                        exit(0);
                    }
                    db_put_num.fetch_add(1);
                    {
                        RunningLockAndErase(headID);
                        /*std::lock_guard<std::mutex> tmplock(m_runningLock);
                        m_splitList.erase(headID);*/

                        SetPostingVersionDefault(p_index, headID);
                    }
                    return ErrorCode::Success;
                }

                long long newHeadVID = -1;
                int first = 0;
                bool theSameHead = false;
                newPostingLists.resize(2);

                if(m_opt->m_enableOptSplit){
                    float dis1 = p_index->ComputeDistance(args.centers, p_index->GetSample(headID));
                    float dis2 = p_index->ComputeDistance(args.centers + args._D, p_index->GetSample(headID));
                    int b = -1, c = -1;
                    int min_count = std::min(args.counts[0],args.counts[1]);
                    //int max_count = std::max(args.counts[0],args.counts[1]);
                    if(dis1 < Epsilon || dis2 < Epsilon) {
                        
                        if(min_count < m_opt->m_mergeThreshold || min_count / static_cast<double>(localIndices.size()) <= m_opt->m_balanceFactor){
                            //LOG(Helper::LogLevel::LL_Info, "Same head\n");
                            //std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);
                            b = args.counts[0] < args.counts[1] ? 0 : 1;
                            c = args.counts[0] < args.counts[1] ? 1 : 0;
                            int start_pos = (b == 0) ? 0 : args.counts[0];
                            int first = (b == 0) ? args.counts[0] : 0;
                            
                            auto* postingP = reinterpret_cast<uint8_t*>(&postingList.front());
                    #pragma omp parallel for schedule(dynamic)
                            for (int j = 0; j < args.counts[b]; j++)
                            {
                                //memcpy(ptr, postingList.c_str() + localIndices[start_pos + j] * m_vectorInfoSize, m_vectorInfoSize);
                                uint8_t* vectorId = postingP + localIndices[start_pos + j] * m_vectorInfoSize;
                                ConcurrentReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), headID);
                                //ConcurrentReassignSingleVector(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), headID, 1);
                            }
                            
                            std::string info;
                            info.resize(args.counts[c] * m_vectorInfoSize);
                            char* ptr = (char*)(info.c_str());
                            std::vector<int> localIndicesId(args.counts[c]);
                            for (int j = 0; j < args.counts[c]; j++, ptr += m_vectorInfoSize)//[first+j, first+ args.counts[k]) is the vector offset of new posting k
                            {
                                memcpy(ptr, postingList.c_str() + localIndices[first + j] * m_vectorInfoSize, m_vectorInfoSize);
                                localIndicesId[j] = localIndices[first + j];
                                //Serialize(ptr, localIndicesInsert[localIndices[first + j]], localIndicesInsertVersion[localIndices[first + j]], smallSample[localIndices[first + j]]);
                            }
                            
                            std::string remain_vecs;
                            newHeadsID.push_back(headID);
                            int vec_count = args.counts[c];
                            m_postingVersionMap->SetNormal(headID);
                            if((remain_vecs = collect(p_index, headID)) != ""){
                                
                                vec_count += remain_vecs.size() / m_vectorInfoSize;                               
                                info.append(remain_vecs);                       
                            }

                            if (db->Put(headID, info) != ErrorCode::Success) {
                                LOG(Helper::LogLevel::LL_Info, "Fail to override postings\n");
                                exit(0);
                            }
                            m_postingSizes.UpdateSize(headID, vec_count);

                            if(vec_count > m_postingSizeLimit){
                                
                                ConcurrentSplitAsync(p_index, headID);
                                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                                double pass_time = (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_record_time).count() * 1.0) / 1000.0;
                                if(pass_time > 5){
                                    ConcurrentDetectAsync(p_index);
                                    last_record_time = now;
                                }
                                return ErrorCode::Success; 
                            }
                            m_stat.m_theSameHeadNum++;                                                       
                            return ErrorCode::Success; 
                        }
                    }

                    
                    
                }



                for (int k = 0; k < 2; k++) {
                    
                    if (args.counts[k] == 0)	continue;//arg.counts[k] is the vector num of centroid (args.centers + k * args._D)
                    
                    newPostingLists[k].resize(args.counts[k] * m_vectorInfoSize);
                    char* ptr = (char*)(newPostingLists[k].c_str());
                    for (int j = 0; j < args.counts[k]; j++, ptr += m_vectorInfoSize)//[first+j, first+ args.counts[k]) is the vector offset of new posting k
                    {
                        memcpy(ptr, postingList.c_str() + localIndices[first + j] * m_vectorInfoSize, m_vectorInfoSize);
                        //Serialize(ptr, localIndicesInsert[localIndices[first + j]], localIndicesInsertVersion[localIndices[first + j]], smallSample[localIndices[first + j]]);
                    }
                    //args.centers is the address of two new centroids
                    //if the new centroid is too closer to the original centroid headID, apply the original centroid headID
                    if (!theSameHead && p_index->ComputeDistance(args.centers + k * args._D, p_index->GetSample(headID)) < Epsilon) {
                        newHeadsID.push_back(headID);
                        newHeadVID = headID;
                        //std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[newHeadVID]);

                        if(k == 0) m_postingVersionMap->SetPID1(headID, newHeadVID);
                        else m_postingVersionMap->SetPID2(headID, newHeadVID);

                        theSameHead = true;
                        auto splitPutBegin = std::chrono::high_resolution_clock::now();
                        if (!preReassign && db->Put(newHeadVID, newPostingLists[k]) != ErrorCode::Success) {
                            LOG(Helper::LogLevel::LL_Info, "Fail to override postings\n");
                            exit(0);
                        }
                        db_put_num.fetch_add(1);
                        auto splitPutEnd = std::chrono::high_resolution_clock::now();
                        elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(splitPutEnd - splitPutBegin).count();
                        m_stat.m_putCost += elapsedMSeconds;
                        m_stat.m_theSameHeadNum++;
                    }
                    else {//the new centroid is different from the original one
                        
                        int begin, end = 0;
                        p_index->AddIndexId(args.centers + k * args._D, 1, m_opt->m_dim, begin, end);//add new centroid to the index
                        newHeadVID = begin;//the new centroid's id
                        std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[newHeadVID]); 
                        std::lock_guard<std::mutex> tmplock(m_dataAddLock);
                        m_postingVersionMap->AddBatch(newHeadVID, 1); 
                        if (m_postingSizes.AddBatch(1) == ErrorCode::MemoryOverFlow) {
                            LOG(Helper::LogLevel::LL_Info, "MemoryOverFlow: NnewHeadVID: %d, Map Size:%d\n", newHeadVID, m_postingSizes.BufferSize());
                            exit(1);
                        }                          
                        
                        if(k == 0) m_postingVersionMap->SetPID1(headID, newHeadVID);
                        else m_postingVersionMap->SetPID2(headID, newHeadVID);

                        newHeadsID.push_back(begin);
                        
                        auto splitPutBegin = std::chrono::high_resolution_clock::now();
                        if (!preReassign && db->Put(newHeadVID, newPostingLists[k]) != ErrorCode::Success) {
                            LOG(Helper::LogLevel::LL_Info, "Fail to add new postings\n");
                            exit(0);
                        }
                        db_put_num.fetch_add(1);
                        auto splitPutEnd = std::chrono::high_resolution_clock::now();
                        elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(splitPutEnd - splitPutBegin).count();
                        m_stat.m_putCost += elapsedMSeconds;
                        auto updateHeadBegin = std::chrono::high_resolution_clock::now();
                        p_index->AddIndexIdx(begin, end);//refine graph: do the operation (end-begin) time(s)
                        auto updateHeadEnd = std::chrono::high_resolution_clock::now();
                        elapsedMSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(updateHeadEnd - updateHeadBegin).count();
                        m_stat.m_updateHeadCost += elapsedMSeconds;

                    }
                    //LOG(Helper::LogLevel::LL_Info, "Head id: %d split into : %d, length: %d\n", headID, newHeadVID, args.counts[k]);
                    first += args.counts[k];
                    m_postingSizes.UpdateSize(newHeadVID, args.counts[k]);
                }

                
                if (reassign) {
                    auto reassignScanBegin = std::chrono::high_resolution_clock::now();                   

                    ConcurrentCollectReAssign(p_index, headID, newPostingLists, newHeadsID);//reassign the original vectors                    

                    auto reassignScanEnd = std::chrono::high_resolution_clock::now();
                    elapsedMSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(reassignScanEnd - reassignScanBegin).count();

                    m_stat.m_reassignScanCost += elapsedMSeconds;
                }

                //SizeType pid1 = m_postingVersionMap->GetPID1(headID);
                //SizeType pid2 = m_postingVersionMap->GetPID2(headID);
                //reassign cache vectors
                std::string cache_vectors;

                if((cache_vectors = collect(p_index, headID)) != ""){
                    ProcessSplitCacheVectors(p_index, cache_vectors, newHeadsID);                     
                }

                {
                    if(m_opt->m_enableFinedLock){
                        std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);//ablation
                    }
                    if (!theSameHead) {                       
                        p_index->DeleteIndex(headID);
                        m_postingSizes.UpdateSize(headID, 0);

                        m_postingVersionMap->Delete(headID);
                    }
                    else{
                        m_postingVersionMap->SetNormal(headID);
                    }
                }
                

                //check if some cache vectors exist
                std::string remain_vecs;
                if((remain_vecs = collect(p_index, headID)) != ""){
                    ProcessSplitCacheVectors(p_index, remain_vecs, newHeadsID);
                }
            }
            {
                RunningLockAndErase(headID);
                /*std::lock_guard<std::mutex> tmplock(m_runningLock);
                // LOG(Helper::LogLevel::LL_Info,"erase: %d\n", headID);
                m_splitList.erase(headID);*/
            }
            m_stat.m_splitNum++;
            
            auto splitEnd = std::chrono::high_resolution_clock::now();
            elapsedMSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(splitEnd - splitBegin).count();
            m_stat.m_splitCost += elapsedMSeconds;
            

            return ErrorCode::Success;
        }

        inline void ProcessSplitCacheVectors(VectorIndex* p_index, std::string& cache_vectors, std::vector<SizeType>& newHeadsID){
            uint8_t* front_addr = reinterpret_cast<uint8_t*>(&cache_vectors.front());
            int cur_vec_num = cache_vectors.size() / m_vectorInfoSize;
            //std::string posting1, posting2;           
            //int num1 = 0, num2 = 0;

            std::vector<std::string> postings(newHeadsID.size(), "");
            std::vector<int> numbers(newHeadsID.size(), 0);

            //LOG(Helper::LogLevel::LL_Info, "cache_vectors' size =%d\n", cache_vectors.size());
            //std::vector<std::string> equal_vectors;
            for (size_t i = 0; i < cur_vec_num; i++)
            {
                uint8_t* vector_info = front_addr + i * m_vectorInfoSize;
                
                SizeType vid = *(reinterpret_cast<SizeType*>(vector_info));
                uint8_t version = *(reinterpret_cast<uint8_t*>(vector_info + sizeof(int)));
                if (m_versionMap->Deleted(vid) || m_versionMap->GetVersion(vid) != version) {
                    continue;
                }

                ValueType* vector_raw_data = reinterpret_cast<ValueType*>(vector_info + m_metaDataSize);

                float distance = -1.0;
                int mark = -1;
                for(int j = 0; j < newHeadsID.size(); j++){
                    float d = p_index->ComputeDistance(vector_raw_data, p_index->GetSample(newHeadsID[j]));
                    if(distance == -1.0){
                        distance = d;
                        mark = j;
                    }
                    else{
                        if(d < distance){
                            distance = d;
                            mark = j;
                        }
                    }
                }

                std::string cur_vec_info(m_vectorInfoSize, '\0');
                char* cur_p = (char*)(cur_vec_info.c_str());
                memcpy(cur_p, vector_info, m_vectorInfoSize);

                postings[mark].append(cur_vec_info);
                numbers[mark]++;
            }
            
            for(int j = 0; j < numbers.size(); j++){
                if(numbers[j] > 0){
                    //ConcurrentAppendAsync(p_index, std::make_shared<std::string>(reinterpret_cast<char*>(&postings[j].front()), numbers[j] * m_vectorInfoSize), newHeadsID[j], numbers[j]);
                    ConcurrentAppend(p_index, newHeadsID[j], numbers[j], postings[j]);
                }
            }
            
            
        }

        inline void ProcessMergeCacheVectors(VectorIndex* p_index, std::string& cache_vectors, SizeType& merged_pid){
            uint8_t* front_addr = reinterpret_cast<uint8_t*>(&cache_vectors.front());
            int cur_vec_num = cache_vectors.size() / m_vectorInfoSize;
            std::string posting;
            int num = 0;

            //LOG(Helper::LogLevel::LL_Info, "cache_vectors' size =%d\n", cache_vectors.size());
            for (size_t i = 0; i < cur_vec_num; i++)
            {
                uint8_t* vector_info = front_addr + i * m_vectorInfoSize;

                SizeType vid = *(reinterpret_cast<SizeType*>(vector_info));
                uint8_t version = *(reinterpret_cast<uint8_t*>(vector_info + sizeof(int)));
                if (m_versionMap->Deleted(vid) || m_versionMap->GetVersion(vid) != version) {
                    continue;
                }

                /*if(vectorIdSet.find(vid) != vectorIdSet.end()){
                    continue;
                }
                vectorIdSet.insert(vid);*/

                std::string cur_vec_info(m_vectorInfoSize, '\0');
                char* cur_p = (char*)(cur_vec_info.c_str());
                memcpy(cur_p, vector_info, m_vectorInfoSize);
                posting.append(cur_vec_info);
                num++;
                //LOG(Helper::LogLevel::LL_Info, "i=%d, cur_vec_info: %s\n", i, cur_vec_info.c_str());         
            }

            
            if(num > 0) {
                //PostingHandle(p_index, merged_pid, num, posting);
                //ConcurrentAppend(p_index, merged_pid, num, posting);
                /*auto func = [&](){
                    ConcurrentAppend(p_index, merged_pid, num, posting);
                };

                std::thread thread(func);
                thread.join();*/
                ConcurrentAppendAsync(p_index, std::make_shared<std::string>(reinterpret_cast<char*>(&posting.front()), num * m_vectorInfoSize), merged_pid, num);
            }

        }

        ErrorCode MergePostings(VectorIndex* p_index, SizeType headID, bool reassign = false)
        {
            {
                if (!m_mergeLock.try_lock()) {
                    auto* curJob = new MergeAsyncJob(p_index, this, headID, reassign, nullptr);
                    m_splitThreadPool->add(curJob);
                    return ErrorCode::Success;
                }
                std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);

                if (!p_index->ContainSample(headID)) {//if bkt index doesn't contain headID vector(the posting centroid) 
                    m_mergeLock.unlock();
                    return ErrorCode::Success;
                }

                std::string mergedPostingList;
                std::set<SizeType> vectorIdSet;

                std::string currentPostingList;
                if (db->Get(headID, &currentPostingList) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "Fail to get to be merged postings: %d\n", headID);
                    exit(0);
                }

                auto* postingP = reinterpret_cast<uint8_t*>(&currentPostingList.front());
                size_t postVectorNum = currentPostingList.size() / m_vectorInfoSize;
                int currentLength = 0;
                uint8_t* vectorId = postingP;
                for (int j = 0; j < postVectorNum; j++, vectorId += m_vectorInfoSize)
                {
                    int VID = *((int*)(vectorId));
                    uint8_t version = *(vectorId + sizeof(int));
                    if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) continue;
                    vectorIdSet.insert(VID);
                    mergedPostingList += currentPostingList.substr(j * m_vectorInfoSize, m_vectorInfoSize);
                    currentLength++;
                }
                int totalLength = currentLength;

                if (currentLength > m_mergeThreshold)
                {
                    m_postingSizes.UpdateSize(headID, currentLength);
                    if (db->Put(headID, mergedPostingList) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Info, "Merge Fail to write back postings\n");
                        exit(0);
                    }
                    m_mergeList.erase(headID);
                    m_mergeLock.unlock();
                    return ErrorCode::Success;
                }

                QueryResult queryResults(p_index->GetSample(headID), m_opt->m_internalResultNum, false);
                p_index->SearchIndex(queryResults);

                std::string nextPostingList;

                for (int i = 1; i < queryResults.GetResultNum(); ++i)
                {
                    BasicResult* queryResult = queryResults.GetResult(i);
                    int nextLength = m_postingSizes.GetSize(queryResult->VID);
                    tbb::concurrent_hash_map<SizeType, SizeType>::const_accessor headIDAccessor;
                    if (currentLength + nextLength < m_postingSizeLimit && !m_mergeList.find(headIDAccessor, queryResult->VID))
                    {
                        {
                            std::unique_lock<std::shared_timed_mutex> anotherLock(m_rwLocks[queryResult->VID], std::defer_lock);
                            //LOG(Helper::LogLevel::LL_Info,"Locked: %d, to be lock: %d\n", headID, queryResult->VID);
                            if (m_rwLocks.hash_func(queryResult->VID) != m_rwLocks.hash_func(headID)) anotherLock.lock();
                            if (!p_index->ContainSample(queryResult->VID)) continue;
                            if (db->Get(queryResult->VID, &nextPostingList) != ErrorCode::Success) {
                                LOG(Helper::LogLevel::LL_Info, "Fail to get to be merged postings: %d\n", queryResult->VID);
                                exit(0);
                            }

                            postingP = reinterpret_cast<uint8_t*>(&nextPostingList.front());
                            postVectorNum = nextPostingList.size() / m_vectorInfoSize;
                            nextLength = 0;
                            vectorId = postingP;
                            for (int j = 0; j < postVectorNum; j++, vectorId += m_vectorInfoSize)
                            {
                                int VID = *((int*)(vectorId));
                                uint8_t version = *(vectorId + sizeof(int));
                                if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) continue;
                                if (vectorIdSet.find(VID) == vectorIdSet.end()) {
                                    mergedPostingList += nextPostingList.substr(j * m_vectorInfoSize, m_vectorInfoSize);
                                    totalLength++;
                                }
                                nextLength++;
                            }
                            if (currentLength > nextLength) 
                            {
                                p_index->DeleteIndex(queryResult->VID);//marked as deleted.set the mark bit of vector whose id equals to queryResult->VID to 1
                                if (db->Put(headID, mergedPostingList) != ErrorCode::Success) {
                                    LOG(Helper::LogLevel::LL_Info, "Split fail to override postings after merge\n");
                                    exit(0);
                                }
                                m_postingSizes.UpdateSize(queryResult->VID, 0);
                                m_postingSizes.UpdateSize(headID, totalLength);
                            } else
                            {
                                p_index->DeleteIndex(headID);
                                if (db->Put(queryResult->VID, mergedPostingList) != ErrorCode::Success) {
                                    LOG(Helper::LogLevel::LL_Info, "Split fail to override postings after merge\n");
                                    exit(0);
                                }
                                m_postingSizes.UpdateSize(queryResult->VID, totalLength);
                                m_postingSizes.UpdateSize(headID, 0);
                            }
                            if (m_rwLocks.hash_func(queryResult->VID) != m_rwLocks.hash_func(headID)) anotherLock.unlock();
                        }

                        // LOG(Helper::LogLevel::LL_Info,"Release: %d, Release: %d\n", headID, queryResult->VID);
                        lock.unlock();
                        m_mergeLock.unlock();

                        if (reassign) 
                        {
                            /* ReAssign */
                            if (currentLength > nextLength) 
                            {
                                /* ReAssign queryResult->VID*/
                                postingP = reinterpret_cast<uint8_t*>(&nextPostingList.front());
                                for (int j = 0; j < nextLength; j++) {
                                    uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                                    // SizeType vid = *(reinterpret_cast<SizeType*>(vectorId));
                                    ValueType* vector = reinterpret_cast<ValueType*>(vectorId + m_metaDataSize);
                                    float origin_dist = p_index->ComputeDistance(p_index->GetSample(queryResult->VID), vector);
                                    float current_dist = p_index->ComputeDistance(p_index->GetSample(headID), vector);
                                    if (current_dist > origin_dist)
                                        ReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), headID);
                                }
                            } else
                            {
                                /* ReAssign headID*/
                                postingP = reinterpret_cast<uint8_t*>(&currentPostingList.front());
                                for (int j = 0; j < currentLength; j++) {
                                    uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                                    // SizeType vid = *(reinterpret_cast<SizeType*>(vectorId));
                                    ValueType* vector = reinterpret_cast<ValueType*>(vectorId + m_metaDataSize);
                                    float origin_dist = p_index->ComputeDistance(p_index->GetSample(headID), vector);
                                    float current_dist = p_index->ComputeDistance(p_index->GetSample(queryResult->VID), vector);
                                    if (current_dist > origin_dist)
                                        ReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), queryResult->VID);
                                }
                            }
                        }

                        m_mergeList.erase(headID);
                        m_stat.m_mergeNum++;

                        return ErrorCode::Success;
                    }
                }
                m_postingSizes.UpdateSize(headID, currentLength);
                if (db->Put(headID, mergedPostingList) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "Merge Fail to write back postings\n");
                    exit(0);
                }
                m_mergeList.erase(headID);
                m_mergeLock.unlock();
            }
            return ErrorCode::Success;
        }

        ErrorCode ConcurrentMergePostings(VectorIndex* p_index, SizeType headID, bool reassign = false)
        {
            {
                if (!m_mergeLock.try_lock()) {//only one merge process is excuted
                    auto* curJob = new MergeAsyncJob(p_index, this, headID, reassign, nullptr);
                    m_splitThreadPool->add(curJob);
                    return ErrorCode::Success;
                }
                //std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);
                

                if (!p_index->ContainSample(headID)) {//if bkt index doesn't contain headID vector(the posting centroid)
                    m_mergeLock.unlock();
                    m_postingVersionMap->Delete(headID);
                    return ErrorCode::Success;
                }

                m_postingVersionMap->Merge(headID);

                std::string mergedPostingList;
                std::set<SizeType> vectorIdSet;

                std::string currentPostingList;
                if (db->Get(headID, &currentPostingList) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "Fail to get to be merged postings: %d\n", headID);
                    exit(0);
                }
                db_get_num.fetch_add(1);

                auto* postingP = reinterpret_cast<uint8_t*>(&currentPostingList.front());
                size_t postVectorNum = currentPostingList.size() / m_vectorInfoSize;
                int currentLength = 0;
                uint8_t* vectorId = postingP;
                for (int j = 0; j < postVectorNum; j++, vectorId += m_vectorInfoSize)
                {
                    int VID = *((int*)(vectorId));
                    uint8_t version = *(vectorId + sizeof(int));
                    if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) continue;
                    vectorIdSet.insert(VID);
                    mergedPostingList += currentPostingList.substr(j * m_vectorInfoSize, m_vectorInfoSize);
                    currentLength++;
                }
                int totalLength = currentLength;

                if (currentLength > m_mergeThreshold)
                {
                    std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);
                    m_postingSizes.UpdateSize(headID, currentLength);
                    if (db->Put(headID, mergedPostingList) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Info, "Merge Fail to write back postings\n");
                        exit(0);
                    }
                    db_put_num.fetch_add(1);
                    m_mergeList.erase(headID);
                    m_postingVersionMap->SetNormal(headID);
                    m_mergeLock.unlock();
                    return ErrorCode::Success;
                }

                QueryResult queryResults(p_index->GetSample(headID), m_opt->m_internalResultNum, false);
                p_index->SearchIndex(queryResults);//obtain the nearest top m_opt->m_internalResultNum postings

                std::string nextPostingList;

                for (int i = 1; i < queryResults.GetResultNum(); ++i)
                {
                    BasicResult* queryResult = queryResults.GetResult(i);
                    int nextLength = m_postingSizes.GetSize(queryResult->VID);//another posting's length
                    tbb::concurrent_hash_map<SizeType, SizeType>::const_accessor headIDAccessor;
                    if (currentLength + nextLength < m_postingSizeLimit && !m_mergeList.find(headIDAccessor, queryResult->VID))
                    {// find a posting that their total length is less than the threshold and the posting is not in the mergeList
                        {
                            /*std::unique_lock<std::shared_timed_mutex> anotherLock(m_rwLocks[queryResult->VID], std::defer_lock);
                            //LOG(Helper::LogLevel::LL_Info,"Locked: %d, to be lock: %d\n", headID, queryResult->VID);
                            if (m_rwLocks.hash_func(queryResult->VID) != m_rwLocks.hash_func(headID)) anotherLock.lock();*/
                            if (!p_index->ContainSample(queryResult->VID)) continue;//if the posting is marked as deleted, skip it
                            m_postingVersionMap->Merge(queryResult->VID);
                            if (db->Get(queryResult->VID, &nextPostingList) != ErrorCode::Success) {
                                LOG(Helper::LogLevel::LL_Info, "Fail to get to be merged postings: %d\n", queryResult->VID);
                                exit(0);
                            }
                            db_get_num.fetch_add(1);

                            postingP = reinterpret_cast<uint8_t*>(&nextPostingList.front());
                            postVectorNum = nextPostingList.size() / m_vectorInfoSize;
                            nextLength = 0;
                            vectorId = postingP;
                            for (int j = 0; j < postVectorNum; j++, vectorId += m_vectorInfoSize)
                            {
                                int VID = *((int*)(vectorId));
                                uint8_t version = *(vectorId + sizeof(int));
                                if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) continue;
                                if (vectorIdSet.find(VID) == vectorIdSet.end()) {//if the vector is not in the posting headID
                                    mergedPostingList += nextPostingList.substr(j * m_vectorInfoSize, m_vectorInfoSize);
                                    totalLength++;
                                }
                                nextLength++;
                            }
                            //mergedPostingList is the merged vectors of the two postings(headID and queryResult->VID), each vector is unique because the duplicate vectors are filtered by vectorIdSet
                            if (currentLength > nextLength) 
                            {
                                std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);
                                m_postingVersionMap->SetPID2(queryResult->VID, headID);
                                p_index->DeleteIndex(queryResult->VID);//marked as deleted. Set the mark bit of vector whose id equals to queryResult->VID to 1
                                m_postingVersionMap->Delete(queryResult->VID);
                                if (db->Put(headID, mergedPostingList) != ErrorCode::Success) {
                                    LOG(Helper::LogLevel::LL_Info, "Split fail to override postings after merge\n");
                                    exit(0);
                                }
                                db_put_num.fetch_add(1);
                                m_postingSizes.UpdateSize(queryResult->VID, 0);
                                m_postingSizes.UpdateSize(headID, totalLength);
                                m_postingVersionMap->SetNormal(headID);
                            } else
                            {
                                std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[queryResult->VID]);
                                m_postingVersionMap->SetPID2(headID, queryResult->VID);
                                p_index->DeleteIndex(headID);
                                m_postingVersionMap->Delete(headID);
                                if (db->Put(queryResult->VID, mergedPostingList) != ErrorCode::Success) {
                                    LOG(Helper::LogLevel::LL_Info, "Split fail to override postings after merge\n");
                                    exit(0);
                                }
                                db_put_num.fetch_add(1);
                                m_postingSizes.UpdateSize(queryResult->VID, totalLength);
                                m_postingSizes.UpdateSize(headID, 0);
                                m_postingVersionMap->SetNormal(queryResult->VID);
                            }
                            //if (m_rwLocks.hash_func(queryResult->VID) != m_rwLocks.hash_func(headID)) anotherLock.unlock();
                        }

                        // LOG(Helper::LogLevel::LL_Info,"Release: %d, Release: %d\n", headID, queryResult->VID);
                        //lock.unlock();
                        m_mergeLock.unlock();                     

                        if (reassign) 
                        {
                            /* ReAssign */
                            if (currentLength > nextLength) 
                            {                                 
                                /* ReAssign queryResult->VID*/
                                postingP = reinterpret_cast<uint8_t*>(&nextPostingList.front());
                                for (int j = 0; j < nextLength; j++) {
                                    uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                                    // SizeType vid = *(reinterpret_cast<SizeType*>(vectorId));
                                    ValueType* vector = reinterpret_cast<ValueType*>(vectorId + m_metaDataSize);
                                    float origin_dist = p_index->ComputeDistance(p_index->GetSample(queryResult->VID), vector);
                                    float current_dist = p_index->ComputeDistance(p_index->GetSample(headID), vector);
                                    if (current_dist > origin_dist)
                                        ConcurrentReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), headID);
                                        //ReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), headID);
                                }                               

                            } 
                            else
                            {
                                /* ReAssign headID*/
                                postingP = reinterpret_cast<uint8_t*>(&currentPostingList.front());
                                for (int j = 0; j < currentLength; j++) {
                                    uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                                    // SizeType vid = *(reinterpret_cast<SizeType*>(vectorId));
                                    ValueType* vector = reinterpret_cast<ValueType*>(vectorId + m_metaDataSize);
                                    float origin_dist = p_index->ComputeDistance(p_index->GetSample(headID), vector);
                                    float current_dist = p_index->ComputeDistance(p_index->GetSample(queryResult->VID), vector);
                                    if (current_dist > origin_dist)
                                        ConcurrentReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), queryResult->VID);
                                        //ReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), queryResult->VID);
                                }
                                
                            }
                        }


                        if(currentLength > nextLength){
                            std::string cache_vectors;

                            //process queryResult->VID cached vectors
                            if((cache_vectors = collect(p_index, queryResult->VID)) != ""){
                                ProcessMergeCacheVectors(p_index, cache_vectors, headID/*, vectorIdSet*/);
                            }

                            //process headID cachedVectors
                            if((cache_vectors = collect(p_index, headID)) != ""){
                                ProcessMergeCacheVectors(p_index, cache_vectors, headID);
                            }
                        }
                        else{
                            std::string cache_vectors;
                                
                            //process headID cached vectors
                            if((cache_vectors = collect(p_index, headID)) != ""){
                                ProcessMergeCacheVectors(p_index, cache_vectors, queryResult->VID);
                            }

                            //process queryResult->VID cachedVectors
                            if((cache_vectors = collect(p_index, queryResult->VID)) != ""){
                                ProcessMergeCacheVectors(p_index, cache_vectors, queryResult->VID);
                            }
                        }

                        m_mergeList.erase(headID);
                        m_stat.m_mergeNum++;

                        return ErrorCode::Success;
                    }
                } 

                std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]);
                m_postingSizes.UpdateSize(headID, currentLength);
                if (db->Put(headID, mergedPostingList) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "Merge Fail to write back postings\n");
                    exit(0);
                }
                db_put_num.fetch_add(1);

                std::string origin_cache_vectors;
                //process headID cachedVectors
                if((origin_cache_vectors = collect(p_index, headID)) != ""){
                    ProcessMergeCacheVectors(p_index, origin_cache_vectors, headID);
                }

                m_postingVersionMap->SetNormal(headID);

                if((origin_cache_vectors = collect(p_index, headID)) != ""){
                    ProcessMergeCacheVectors(p_index, origin_cache_vectors, headID);
                }

                m_mergeList.erase(headID);
                m_mergeLock.unlock();
            }
            return ErrorCode::Success;
        }

        inline void ConcurrentSplitAsync(VectorIndex* p_index, SizeType headID, std::function<void()> p_callback = nullptr)
        {
            // LOG(Helper::LogLevel::LL_Info,"Into SplitAsync, current headID: %d, size: %d\n", headID, m_postingSizes.GetSize(headID));
            // tbb::concurrent_hash_map<SizeType, SizeType>::const_accessor headIDAccessor;
            // if (m_splitList.find(headIDAccessor, headID)) {
            //     return;
            // }
            // tbb::concurrent_hash_map<SizeType, SizeType>::value_type workPair(headID, headID);
            // m_splitList.insert(workPair);
            {
                std::lock_guard<std::mutex> tmplock(m_runningLock);

                if (m_splitList.find(headID) != m_splitList.end()) {
                    // LOG(Helper::LogLevel::LL_Info,"Already in queue\n");
                    return;
                }
                m_splitList.insert(headID);
            }

            auto* curJob = new ConcurrentSplitAsyncJob(p_index, this, headID, m_opt->m_disableReassign, p_callback);
            m_splitThreadPool->add(curJob);
            // LOG(Helper::LogLevel::LL_Info, "Add to thread pool\n");
        }

        inline void SplitAsync(VectorIndex* p_index, SizeType headID, std::function<void()> p_callback = nullptr)
        {
            // LOG(Helper::LogLevel::LL_Info,"Into SplitAsync, current headID: %d, size: %d\n", headID, m_postingSizes.GetSize(headID));
            // tbb::concurrent_hash_map<SizeType, SizeType>::const_accessor headIDAccessor;
            // if (m_splitList.find(headIDAccessor, headID)) {
            //     return;
            // }
            // tbb::concurrent_hash_map<SizeType, SizeType>::value_type workPair(headID, headID);
            // m_splitList.insert(workPair);
            {
                std::lock_guard<std::mutex> tmplock(m_runningLock);

                if (m_splitList.find(headID) != m_splitList.end()) {
                    // LOG(Helper::LogLevel::LL_Info,"Already in queue\n");
                    return;
                }
                m_splitList.insert(headID);
            }

            auto* curJob = new SplitAsyncJob(p_index, this, headID, m_opt->m_disableReassign, p_callback);
            m_splitThreadPool->add(curJob);
            // LOG(Helper::LogLevel::LL_Info, "Add to thread pool\n");
        }

        inline void ConcurrentMergeAsync(VectorIndex* p_index, SizeType headID, std::function<void()> p_callback = nullptr)
        {
            if (!m_opt->m_update) return;
            tbb::concurrent_hash_map<SizeType, SizeType>::const_accessor headIDAccessor;
            if (m_mergeList.find(headIDAccessor, headID)) {
                return;
            }
            tbb::concurrent_hash_map<SizeType, SizeType>::value_type workPair(headID, headID);
            m_mergeList.insert(workPair);

            auto* curJob = new ConcurrentMergeAsyncJob(p_index, this, headID, m_opt->m_disableReassign, p_callback);
            m_splitThreadPool->add(curJob);
        }

        inline void MergeAsync(VectorIndex* p_index, SizeType headID, std::function<void()> p_callback = nullptr)
        {
            if (!m_opt->m_update) return;
            tbb::concurrent_hash_map<SizeType, SizeType>::const_accessor headIDAccessor;
            if (m_mergeList.find(headIDAccessor, headID)) {
                return;
            }
            tbb::concurrent_hash_map<SizeType, SizeType>::value_type workPair(headID, headID);
            m_mergeList.insert(workPair);

            auto* curJob = new MergeAsyncJob(p_index, this, headID, m_opt->m_disableReassign, p_callback);
            m_splitThreadPool->add(curJob);
        }

        inline void ConcurrentReassignAsync(VectorIndex* p_index, std::shared_ptr<std::string> vectorInfo, SizeType HeadPrev, std::function<void()> p_callback = nullptr)
        {
            auto* curJob = new ConcurrentReassignAsyncJob(p_index, this, std::move(vectorInfo), HeadPrev, p_callback);
            m_splitThreadPool->add(curJob);
        }

        inline void ReassignAsync(VectorIndex* p_index, std::shared_ptr<std::string> vectorInfo, SizeType HeadPrev, std::function<void()> p_callback = nullptr)
        {
            auto* curJob = new ReassignAsyncJob(p_index, this, std::move(vectorInfo), HeadPrev, p_callback);
            m_splitThreadPool->add(curJob);
        }

        inline void ConcurrentAppendAsync(VectorIndex* p_index, std::shared_ptr<std::string> vectorInfo, SizeType HeadID, int appendNum, std::function<void()> p_callback = nullptr)
        {
            auto* curJob = new ConcurrentAppendAsyncJob(p_index, this, vectorInfo, HeadID, appendNum, p_callback);
            m_splitThreadPool->add(curJob);
        }

        ErrorCode ConcurrentCollectReAssign(VectorIndex* p_index, SizeType headID, std::vector<std::string>& postingLists, std::vector<SizeType>& newHeadsID) {
            auto headVector = reinterpret_cast<const ValueType*>(p_index->GetSample(headID));
            std::vector<float> newHeadsDist;
            std::set<SizeType> reAssignVectorsTopK;
            newHeadsDist.push_back(p_index->ComputeDistance(p_index->GetSample(headID), p_index->GetSample(newHeadsID[0])));
            newHeadsDist.push_back(p_index->ComputeDistance(p_index->GetSample(headID), p_index->GetSample(newHeadsID[1])));
            for (int i = 0; i < postingLists.size(); i++) {
                auto& postingList = postingLists[i];
                size_t postVectorNum = postingList.size() / m_vectorInfoSize;
                auto* postingP = reinterpret_cast<uint8_t*>(&postingList.front());
                for (int j = 0; j < postVectorNum; j++) {
                    uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                    SizeType vid = *(reinterpret_cast<SizeType*>(vectorId));
                    // LOG(Helper::LogLevel::LL_Info, "VID: %d, Head: %d\n", vid, newHeadsID[i]);
                    uint8_t version = *(reinterpret_cast<uint8_t*>(vectorId + sizeof(int)));
                    ValueType* vector = reinterpret_cast<ValueType*>(vectorId + m_metaDataSize);
                    if (reAssignVectorsTopK.find(vid) == reAssignVectorsTopK.end() && !m_versionMap->Deleted(vid) && m_versionMap->GetVersion(vid) == version) {
                        m_stat.m_reAssignScanNum++;
                        float dist = p_index->ComputeDistance(p_index->GetSample(newHeadsID[i]), vector);
                        if (CheckIsNeedReassign(p_index, newHeadsID, vector, headID, newHeadsDist[i], dist, true, newHeadsID[i])) {
                            ConcurrentReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), newHeadsID[i]);
                            reAssignVectorsTopK.insert(vid);
                        }
                    }
                }
            }
            if (m_opt->m_reassignK > 0) {
                std::vector<SizeType> HeadPrevTopK;
                newHeadsDist.clear();
                newHeadsDist.resize(0);
                postingLists.clear();
                postingLists.resize(0);
                COMMON::QueryResultSet<ValueType> nearbyHeads(headVector, m_opt->m_reassignK);
                p_index->SearchIndex(nearbyHeads);
                BasicResult* queryResults = nearbyHeads.GetResults();
                for (int i = 0; i < nearbyHeads.GetResultNum(); i++) {
                    auto vid = queryResults[i].VID;
                    if (vid == -1) break;

                    if (find(newHeadsID.begin(), newHeadsID.end(), vid) == newHeadsID.end()) {
                        HeadPrevTopK.push_back(vid);
                        newHeadsID.push_back(vid);
                        newHeadsDist.push_back(queryResults[i].Dist);
                    }
                }
                auto reassignScanIOBegin = std::chrono::high_resolution_clock::now();
                if (db->MultiGet(HeadPrevTopK, &postingLists) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "ReAssign can't get all the near postings\n");
                    exit(0);
                }
                db_get_num.fetch_add(postingLists.size());
                auto reassignScanIOEnd = std::chrono::high_resolution_clock::now();
                auto elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(reassignScanIOEnd - reassignScanIOBegin).count();
                m_stat.m_reassignScanIOCost += elapsedMSeconds;

                for (int i = 0; i < postingLists.size(); i++) {
                    auto& postingList = postingLists[i];
                    size_t postVectorNum = postingList.size() / m_vectorInfoSize;
                    auto* postingP = reinterpret_cast<uint8_t*>(&postingList.front());
                    for (int j = 0; j < postVectorNum; j++) {
                        uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                        SizeType vid = *(reinterpret_cast<SizeType*>(vectorId));
                        // LOG(Helper::LogLevel::LL_Info, "%d: VID: %d, Head: %d, size:%d/%d\n", i, vid, HeadPrevTopK[i], postingLists.size(), HeadPrevTopK.size());
                        uint8_t version = *(reinterpret_cast<uint8_t*>(vectorId + sizeof(int)));
                        ValueType* vector = reinterpret_cast<ValueType*>(vectorId + m_metaDataSize);
                        if (reAssignVectorsTopK.find(vid) == reAssignVectorsTopK.end() && !m_versionMap->Deleted(vid) && m_versionMap->GetVersion(vid) == version) {
                            m_stat.m_reAssignScanNum++;
                            float dist = p_index->ComputeDistance(p_index->GetSample(HeadPrevTopK[i]), vector);
                            if (CheckIsNeedReassign(p_index, newHeadsID, vector, headID, newHeadsDist[i], dist, false, HeadPrevTopK[i])) {
                                ConcurrentReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), HeadPrevTopK[i]);
                                reAssignVectorsTopK.insert(vid);
                            }
                        }
                    }
                }
            }
            // exit(1);
            return ErrorCode::Success;
        }

        ErrorCode CollectReAssign(VectorIndex* p_index, SizeType headID, std::vector<std::string>& postingLists, std::vector<SizeType>& newHeadsID) {
            auto headVector = reinterpret_cast<const ValueType*>(p_index->GetSample(headID));
            std::vector<float> newHeadsDist;
            std::set<SizeType> reAssignVectorsTopK;
            newHeadsDist.push_back(p_index->ComputeDistance(p_index->GetSample(headID), p_index->GetSample(newHeadsID[0])));
            newHeadsDist.push_back(p_index->ComputeDistance(p_index->GetSample(headID), p_index->GetSample(newHeadsID[1])));
            for (int i = 0; i < postingLists.size(); i++) {
                auto& postingList = postingLists[i];
                size_t postVectorNum = postingList.size() / m_vectorInfoSize;
                auto* postingP = reinterpret_cast<uint8_t*>(&postingList.front());
                for (int j = 0; j < postVectorNum; j++) {
                    uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                    SizeType vid = *(reinterpret_cast<SizeType*>(vectorId));
                    // LOG(Helper::LogLevel::LL_Info, "VID: %d, Head: %d\n", vid, newHeadsID[i]);
                    uint8_t version = *(reinterpret_cast<uint8_t*>(vectorId + sizeof(int)));
                    ValueType* vector = reinterpret_cast<ValueType*>(vectorId + m_metaDataSize);
                    if (reAssignVectorsTopK.find(vid) == reAssignVectorsTopK.end() && !m_versionMap->Deleted(vid) && m_versionMap->GetVersion(vid) == version) {
                        m_stat.m_reAssignScanNum++;
                        float dist = p_index->ComputeDistance(p_index->GetSample(newHeadsID[i]), vector);
                        if (CheckIsNeedReassign(p_index, newHeadsID, vector, headID, newHeadsDist[i], dist, true, newHeadsID[i])) {
                            ReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), newHeadsID[i]);
                            reAssignVectorsTopK.insert(vid);
                        }
                    }
                }
            }
            if (m_opt->m_reassignK > 0) {
                std::vector<SizeType> HeadPrevTopK;
                newHeadsDist.clear();
                newHeadsDist.resize(0);
                postingLists.clear();
                postingLists.resize(0);
                COMMON::QueryResultSet<ValueType> nearbyHeads(headVector, m_opt->m_reassignK);
                p_index->SearchIndex(nearbyHeads);
                BasicResult* queryResults = nearbyHeads.GetResults();
                for (int i = 0; i < nearbyHeads.GetResultNum(); i++) {
                    auto vid = queryResults[i].VID;
                    if (vid == -1) break;

                    if (find(newHeadsID.begin(), newHeadsID.end(), vid) == newHeadsID.end()) {
                        HeadPrevTopK.push_back(vid);
                        newHeadsID.push_back(vid);
                        newHeadsDist.push_back(queryResults[i].Dist);
                    }
                }
                auto reassignScanIOBegin = std::chrono::high_resolution_clock::now();
                if (db->MultiGet(HeadPrevTopK, &postingLists) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "ReAssign can't get all the near postings\n");
                    exit(0);
                }
                auto reassignScanIOEnd = std::chrono::high_resolution_clock::now();
                auto elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(reassignScanIOEnd - reassignScanIOBegin).count();
                m_stat.m_reassignScanIOCost += elapsedMSeconds;

                for (int i = 0; i < postingLists.size(); i++) {
                    auto& postingList = postingLists[i];
                    size_t postVectorNum = postingList.size() / m_vectorInfoSize;
                    auto* postingP = reinterpret_cast<uint8_t*>(&postingList.front());
                    for (int j = 0; j < postVectorNum; j++) {
                        uint8_t* vectorId = postingP + j * m_vectorInfoSize;
                        SizeType vid = *(reinterpret_cast<SizeType*>(vectorId));
                        // LOG(Helper::LogLevel::LL_Info, "%d: VID: %d, Head: %d, size:%d/%d\n", i, vid, HeadPrevTopK[i], postingLists.size(), HeadPrevTopK.size());
                        uint8_t version = *(reinterpret_cast<uint8_t*>(vectorId + sizeof(int)));
                        ValueType* vector = reinterpret_cast<ValueType*>(vectorId + m_metaDataSize);
                        if (reAssignVectorsTopK.find(vid) == reAssignVectorsTopK.end() && !m_versionMap->Deleted(vid) && m_versionMap->GetVersion(vid) == version) {
                            m_stat.m_reAssignScanNum++;
                            float dist = p_index->ComputeDistance(p_index->GetSample(HeadPrevTopK[i]), vector);
                            if (CheckIsNeedReassign(p_index, newHeadsID, vector, headID, newHeadsDist[i], dist, false, HeadPrevTopK[i])) {
                                ReassignAsync(p_index, std::make_shared<std::string>((char*)vectorId, m_vectorInfoSize), HeadPrevTopK[i]);
                                reAssignVectorsTopK.insert(vid);
                            }
                        }
                    }
                }
            }
            // exit(1);
            return ErrorCode::Success;
        }

        bool RNGSelection(std::vector<Edge>& selections, ValueType* queryVector, VectorIndex* p_index, SizeType p_fullID, int& replicaCount, int checkHeadID = -1)
        {
            QueryResult queryResults(queryVector, m_opt->m_internalResultNum, false);
            p_index->SearchIndex(queryResults);//queryResults stores the top k centroids in the memory

            replicaCount = 0;
            for (int i = 0; i < queryResults.GetResultNum() && replicaCount < m_opt->m_replicaCount; ++i)
            {
                BasicResult* queryResult = queryResults.GetResult(i);
                if (queryResult->VID == -1) {
                    break;
                }
                // RNG Check.
                bool rngAccpeted = true;
                for (int j = 0; j < replicaCount; ++j)
                {
                    float nnDist = p_index->ComputeDistance(p_index->GetSample(queryResult->VID),
                        p_index->GetSample(selections[j].node));//get the distance of current centroid and queryVector's neighbor with number j
                    if (m_opt->m_rngFactor * nnDist <= queryResult->Dist)//if the neighbor j is closer to the centroid, the centroid cannot be the neighbor of queryVector
                    {
                        rngAccpeted = false;
                        break;
                    }
                }
                if (!rngAccpeted) continue;
                selections[replicaCount].node = queryResult->VID;
                selections[replicaCount].tonode = p_fullID;
                selections[replicaCount].distance = queryResult->Dist;
                if (selections[replicaCount].node == checkHeadID) {
                    return false;
                }
                ++replicaCount;
            }
            return true;
        }

        ErrorCode ConcurrentAppend(VectorIndex* p_index, SizeType headID, int appendNum, std::string& appendPosting, int reassignThreshold = 0)
        {
            auto appendBegin = std::chrono::high_resolution_clock::now();
            if (appendPosting.empty()) {
                LOG(Helper::LogLevel::LL_Error, "Error! empty append posting!\n");
            }

            if (appendNum == 0) {
                LOG(Helper::LogLevel::LL_Info, "Error!, headID :%d, appendNum:%d\n", headID, appendNum);
            }

        checkDeleted:
            if (/*!p_index->ContainSample(headID) ||*/ m_postingVersionMap->Deleted(headID)) {//if posting headID is marked as deleted
                delete_branch++;
                for (int i = 0; i < appendNum; i++)
                {
                    uint32_t idx = i * m_vectorInfoSize;
                    SizeType VID = *(int*)(&appendPosting[idx]);
                    uint8_t version = *(uint8_t*)(&appendPosting[idx + sizeof(int)]);
                    auto vectorInfo = std::make_shared<std::string>(appendPosting.c_str() + idx, m_vectorInfoSize);
                    if (m_versionMap->GetVersion(VID) == version) {
                        // LOG(Helper::LogLevel::LL_Info, "Head Miss To ReAssign: VID: %d, current version: %d\n", *(int*)(&appendPosting[idx]), version);
                        m_stat.m_headMiss++;
                        ConcurrentReassignAsync(p_index, vectorInfo, headID);
                    }
                    // LOG(Helper::LogLevel::LL_Info, "Head Miss Do Not To ReAssign: VID: %d, version: %d, current version: %d\n", *(int*)(&appendPosting[idx]), m_versionMap->GetVersion(*(int*)(&appendPosting[idx])), version);
                }
                return ErrorCode::Undefined;
            }
        //sleepLoop:
            double appendIOSeconds = 0;
            bool needSplit = false;
            {
                //std::shared_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]); //ROCKSDB
                std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]); //SPDK

                //if the posting is marked as deleted
                if (/*!p_index->ContainSample(headID) ||*/ m_postingVersionMap->Deleted(headID)) {
                    goto checkDeleted;
                }

                //if the posting is splitted or merged
                if(m_postingVersionMap->Splitting(headID) || m_postingVersionMap->Merging(headID)){
                    split_merge_branch++;
                    std::string old_val = m_postingVersionMap->getVectorCacheInfo(headID);
                    std::unordered_set<int> vectors_in_cache;

                    uint8_t* front_addr = reinterpret_cast<uint8_t*>(&old_val.front());
                    int vec_num = old_val.size() / m_vectorInfoSize;
                    //LOG(Helper::LogLevel::LL_Info, "posting %d, cache vector number:%d\n", curPostingID, vec_num);
            
                    for(int j = 0; j < vec_num; j++){
                        
                        uint8_t* vector_info = front_addr + j * m_vectorInfoSize;
                        SizeType vid = *(reinterpret_cast<SizeType*>(vector_info));

                        vectors_in_cache.insert(vid);
                    }

                    front_addr = reinterpret_cast<uint8_t*>(&appendPosting.front());
                    
                    std::string filtered_posting;
                    int filter_num = 0;
                    for(int j = 0; j < appendNum; j++){
                        
                        uint8_t* vector_info = front_addr + j * m_vectorInfoSize;
                        SizeType vid = *(reinterpret_cast<SizeType*>(vector_info));

                        if(vectors_in_cache.count(vid)==0){
                            std::string tmp_posting(m_vectorInfoSize, '\0');
                            memcpy((char*)(tmp_posting.c_str()), vector_info, m_vectorInfoSize);
                            filtered_posting.append(tmp_posting);
                            filter_num++;
                        }
                    }

                    m_postingVersionMap->addToVectorCache(headID, filter_num, filtered_posting);
                    return ErrorCode::Success;
                }

                normal_branch++;
                auto appendIOBegin = std::chrono::high_resolution_clock::now();
                
                std::string oldPostingList;
                if (db->Get(headID, &oldPostingList) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "Get old posting value failed!\n");
                    exit(0);
                }
                db_get_num.fetch_add(1);

                if(m_opt->m_filterAppend){
                    std::unordered_set<SizeType> vectorIdSet;
                    std::string filtered_posting;
                    int filtered_num = 0;

                    int old_size = oldPostingList.size() / m_vectorInfoSize;

                    uint8_t* addr = reinterpret_cast<uint8_t*>(&oldPostingList.front());
                    for(int a = 0; a < old_size; a++){
                        uint8_t* c_addr = addr + a * m_vectorInfoSize;
                        //uint8_t version = *(c_addr + sizeof(SizeType));
                        int VID = *((int*)(c_addr));

                        //if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version)
                        vectorIdSet.insert(VID);
                    }

                    addr = reinterpret_cast<uint8_t*>(&appendPosting.front());
                    for(int i = 0; i < appendNum; i++){
                        uint8_t* c_addr = addr + i * m_vectorInfoSize;
                        //uint8_t version = *(c_addr + sizeof(SizeType));
                        int VID = *((int*)(c_addr));
                        
                        if(vectorIdSet.find(VID) == vectorIdSet.end()){
                            vectorIdSet.insert(VID);

                            std::string cur_vec_info(m_vectorInfoSize, '\0');
                            char* cur_p = (char*)(cur_vec_info.c_str());
                            memcpy(cur_p, c_addr, m_vectorInfoSize);

                            filtered_posting.append(cur_vec_info);
                            filtered_num++;
                        }
                    }

                    //LOG(Helper::LogLevel::LL_Info, "appendNum: %d, filteredNum: %d\n", appendNum, filtered_num);
                    //if (db->Merge(headID, appendPosting) != ErrorCode::Success) {
                    //    LOG(Helper::LogLevel::LL_Error, "Merge failed! Posting Size:%d, limit: %d\n", m_postingSizes.GetSize(headID), m_postingSizeLimit);
                    //    GetDBStats();
                    //    exit(1);
                    //}
                    if (db->Put(headID, oldPostingList.append(filtered_posting)) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Error, "Merge failed! Posting Size:%d, limit: %d\n", m_postingSizes.GetSize(headID), m_postingSizeLimit);
                        GetDBStats();
                        exit(1);
                    }
                    m_postingSizes.IncSize(headID, filtered_num);
                }
                else{
                    if (db->Put(headID, oldPostingList.append(appendPosting)) != ErrorCode::Success) {
                        LOG(Helper::LogLevel::LL_Error, "Merge failed! Posting Size:%d, limit: %d\n", m_postingSizes.GetSize(headID), m_postingSizeLimit);
                        GetDBStats();
                        exit(1);
                    }
                    m_postingSizes.IncSize(headID, appendNum);
                }
                /*
                std::set<SizeType> vectorIdSet;
                std::string filtered_posting;
                int filtered_num = 0;

                int old_size = oldPostingList.size() / m_vectorInfoSize;

                uint8_t* addr = reinterpret_cast<uint8_t*>(&oldPostingList.front());
                for(int a = 0; a < old_size; a++){
                    uint8_t* c_addr = addr + a * m_vectorInfoSize;
                    //uint8_t version = *(c_addr + sizeof(SizeType));
                    int VID = *((int*)(c_addr));

                    //if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version)
                    vectorIdSet.insert(VID);
                }

                addr = reinterpret_cast<uint8_t*>(&appendPosting.front());
                for(int i = 0; i < appendNum; i++){
                    uint8_t* c_addr = addr + i * m_vectorInfoSize;
                    //uint8_t version = *(c_addr + sizeof(SizeType));
                    int VID = *((int*)(c_addr));
                    
                    if(vectorIdSet.find(VID) == vectorIdSet.end()){
                        vectorIdSet.insert(VID);

                        std::string cur_vec_info(m_vectorInfoSize, '\0');
                        char* cur_p = (char*)(cur_vec_info.c_str());
                        memcpy(cur_p, c_addr, m_vectorInfoSize);

                        filtered_posting.append(cur_vec_info);
                        filtered_num++;
                    }
                }

                //LOG(Helper::LogLevel::LL_Info, "appendNum: %d, filteredNum: %d\n", appendNum, filtered_num);
                //if (db->Merge(headID, appendPosting) != ErrorCode::Success) {
                //    LOG(Helper::LogLevel::LL_Error, "Merge failed! Posting Size:%d, limit: %d\n", m_postingSizes.GetSize(headID), m_postingSizeLimit);
                //    GetDBStats();
                //    exit(1);
                //}
                if (db->Put(headID, oldPostingList.append(filtered_posting)) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Error, "Merge failed! Posting Size:%d, limit: %d\n", m_postingSizes.GetSize(headID), m_postingSizeLimit);
                    GetDBStats();
                    exit(1);
                }
                if (db->Put(headID, oldPostingList.append(appendPosting)) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Error, "Merge failed! Posting Size:%d, limit: %d\n", m_postingSizes.GetSize(headID), m_postingSizeLimit);
                    GetDBStats();
                    exit(1);
                }*/
                db_put_num.fetch_add(1);
                auto appendIOEnd = std::chrono::high_resolution_clock::now();
                appendIOSeconds = std::chrono::duration_cast<std::chrono::microseconds>(appendIOEnd - appendIOBegin).count();
                
                //m_postingSizes.IncSize(headID, appendNum/*filtered_num*/);//CAS operation that modifies the postingSize

                if (m_postingSizes.GetSize(headID) > (m_postingSizeLimit + reassignThreshold)) {
                    m_postingVersionMap->Split(headID);
                    needSplit = true;
                }
            }

            if (needSplit) {
                // SizeType VID = *(int*)(&appendPosting[0]);
                // LOG(Helper::LogLevel::LL_Error, "Split Triggered by inserting VID: %d, reAssign: %d\n", VID, reassignThreshold);
                // GetDBStats();
                // if (m_postingSizes.GetSize(headID) > 120) {
                //     GetDBStats();
                //     exit(1);
                // }
                
                //ConcurrentSplitAsync(p_index, headID);
                if (!reassignThreshold) ConcurrentSplitAsync(p_index, headID);
                else ConcurrentSplit(p_index, headID, !m_opt->m_disableReassign);
                // SplitAsync(p_index, headID);
            }
            
            auto appendEnd = std::chrono::high_resolution_clock::now();
            double elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(appendEnd - appendBegin).count();
            if (!reassignThreshold) {
                m_stat.m_appendTaskNum++;
                m_stat.m_appendIOCost += appendIOSeconds;
                m_stat.m_appendCost += elapsedMSeconds;
            }
            // } else {
            //     LOG(Helper::LogLevel::LL_Info, "ReAssign Append To: %d\n", headID);
            // }
            return ErrorCode::Success;
        }

        ErrorCode Append(VectorIndex* p_index, SizeType headID, int appendNum, std::string& appendPosting, int reassignThreshold = 0)
        {
            auto appendBegin = std::chrono::high_resolution_clock::now();
            if (appendPosting.empty()) {
                LOG(Helper::LogLevel::LL_Error, "Error! empty append posting!\n");
            }

            if (appendNum == 0) {
                LOG(Helper::LogLevel::LL_Info, "Error!, headID :%d, appendNum:%d\n", headID, appendNum);
            }

        checkDeleted:
            if (!p_index->ContainSample(headID)) {//if posting headID is marked as deleted
                for (int i = 0; i < appendNum; i++)
                {
                    uint32_t idx = i * m_vectorInfoSize;
                    SizeType VID = *(int*)(&appendPosting[idx]);
                    uint8_t version = *(uint8_t*)(&appendPosting[idx + sizeof(int)]);
                    auto vectorInfo = std::make_shared<std::string>(appendPosting.c_str() + idx, m_vectorInfoSize);
                    if (m_versionMap->GetVersion(VID) == version) {
                        // LOG(Helper::LogLevel::LL_Info, "Head Miss To ReAssign: VID: %d, current version: %d\n", *(int*)(&appendPosting[idx]), version);
                        m_stat.m_headMiss++;
                        ReassignAsync(p_index, vectorInfo, headID);
                    }
                    // LOG(Helper::LogLevel::LL_Info, "Head Miss Do Not To ReAssign: VID: %d, version: %d, current version: %d\n", *(int*)(&appendPosting[idx]), m_versionMap->GetVersion(*(int*)(&appendPosting[idx])), version);
                }
                return ErrorCode::Undefined;
            }
            double appendIOSeconds = 0;
            {

                //std::shared_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]); //ROCKSDB
                std::unique_lock<std::shared_timed_mutex> lock(m_rwLocks[headID]); //SPDK               

                if (!p_index->ContainSample(headID)) {
                    goto checkDeleted;
                }
                auto appendIOBegin = std::chrono::high_resolution_clock::now();
                
                std::string oldPostingList;
                if (db->Get(headID, &oldPostingList) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "Get old posting value failed!\n");
                    exit(0);
                }

                /*if (db->Merge(headID, appendPosting) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Error, "Merge failed! Posting Size:%d, limit: %d\n", m_postingSizes.GetSize(headID), m_postingSizeLimit);
                    GetDBStats();
                    exit(1);
                }*/
                if (db->Put(headID, oldPostingList.append(appendPosting)) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Error, "Merge failed! Posting Size:%d, limit: %d\n", m_postingSizes.GetSize(headID), m_postingSizeLimit);
                    GetDBStats();
                    exit(1);
                }
                auto appendIOEnd = std::chrono::high_resolution_clock::now();
                appendIOSeconds = std::chrono::duration_cast<std::chrono::microseconds>(appendIOEnd - appendIOBegin).count();
                m_postingSizes.IncSize(headID, appendNum);//CAS operation that modifies the postingSize
            }
            if (m_postingSizes.GetSize(headID) > (m_postingSizeLimit + reassignThreshold)) {
                // SizeType VID = *(int*)(&appendPosting[0]);
                // LOG(Helper::LogLevel::LL_Error, "Split Triggered by inserting VID: %d, reAssign: %d\n", VID, reassignThreshold);
                // GetDBStats();
                // if (m_postingSizes.GetSize(headID) > 120) {
                //     GetDBStats();
                //     exit(1);
                // }
                if (!reassignThreshold) SplitAsync(p_index, headID);
                else Split(p_index, headID, !m_opt->m_disableReassign);
                // SplitAsync(p_index, headID);
            }
            auto appendEnd = std::chrono::high_resolution_clock::now();
            double elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(appendEnd - appendBegin).count();
            if (!reassignThreshold) {
                m_stat.m_appendTaskNum++;
                m_stat.m_appendIOCost += appendIOSeconds;
                m_stat.m_appendCost += elapsedMSeconds;
            }
            // } else {
            //     LOG(Helper::LogLevel::LL_Info, "ReAssign Append To: %d\n", headID);
            // }
            return ErrorCode::Success;
        }
        
        void Reassign(VectorIndex* p_index, std::shared_ptr<std::string> vectorInfo, SizeType HeadPrev)
        {
            SizeType VID = *((SizeType*)vectorInfo->c_str());
            uint8_t version = *((uint8_t*)(vectorInfo->c_str() + sizeof(VID)));
            // return;
            // LOG(Helper::LogLevel::LL_Info, "ReassignID: %d, version: %d, current version: %d, HeadPrev: %d\n", VID, version, m_versionMap->GetVersion(VID), HeadPrev);
            if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) {
                return;
            }
            auto reassignBegin = std::chrono::high_resolution_clock::now();

            m_stat.m_reAssignNum++;

            auto selectBegin = std::chrono::high_resolution_clock::now();
            std::vector<Edge> selections(static_cast<size_t>(m_opt->m_replicaCount));
            int replicaCount;
            bool isNeedReassign = RNGSelection(selections, (ValueType*)(vectorInfo->c_str() + m_metaDataSize), p_index, VID, replicaCount, HeadPrev);
            auto selectEnd = std::chrono::high_resolution_clock::now();
            auto elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(selectEnd - selectBegin).count();
            m_stat.m_selectCost += elapsedMSeconds;

            auto reassignAppendBegin = std::chrono::high_resolution_clock::now();
            // LOG(Helper::LogLevel::LL_Info, "Need ReAssign\n");
            if (isNeedReassign && m_versionMap->GetVersion(VID) == version) {
                // LOG(Helper::LogLevel::LL_Info, "Update Version: VID: %d, version: %d, current version: %d\n", VID, version, m_versionMap.GetVersion(VID));
                m_versionMap->IncVersion(VID, &version);
                (*vectorInfo)[sizeof(VID)] = version;

                //LOG(Helper::LogLevel::LL_Info, "Reassign: oldVID:%d, replicaCount:%d, candidateNum:%d, dist0:%f\n", oldVID, replicaCount, i, selections[0].distance);
                for (int i = 0; i < replicaCount && m_versionMap->GetVersion(VID) == version; i++) {
                    //LOG(Helper::LogLevel::LL_Info, "Reassign: headID :%d, oldVID:%d, newVID:%d, posting length: %d, dist: %f, string size: %d\n", headID, oldVID, VID, m_postingSizes[headID].load(), selections[i].distance, newPart.size());
                    if (ErrorCode::Undefined == Append(p_index, selections[i].node, 1, *vectorInfo, 3)) {
                        // LOG(Helper::LogLevel::LL_Info, "Head Miss: VID: %d, current version: %d, another re-assign\n", VID, version);
                        break;
                    }
                }
            }
            auto reassignAppendEnd = std::chrono::high_resolution_clock::now();
            elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(reassignAppendEnd - reassignAppendBegin).count();
            m_stat.m_reAssignAppendCost += elapsedMSeconds;

            auto reassignEnd = std::chrono::high_resolution_clock::now();
            elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(reassignEnd - reassignBegin).count();
            m_stat.m_reAssignCost += elapsedMSeconds;
        }

        void ConcurrentReassignSingleVector(VectorIndex* p_index, std::shared_ptr<std::string> vectorInfo, SizeType HeadPrev)
        {
            SizeType VID = *((SizeType*)vectorInfo->c_str());
            uint8_t version = *((uint8_t*)(vectorInfo->c_str() + sizeof(VID)));
            // return;
            // LOG(Helper::LogLevel::LL_Info, "ReassignID: %d, version: %d, current version: %d, HeadPrev: %d\n", VID, version, m_versionMap->GetVersion(VID), HeadPrev);
            if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) {
                return;
            }
            auto reassignBegin = std::chrono::high_resolution_clock::now();

            m_stat.m_reAssignNum++;

            auto selectBegin = std::chrono::high_resolution_clock::now();
            std::vector<Edge> selections(static_cast<size_t>(m_opt->m_replicaCount));
            int replicaCount;
            bool isNeedReassign = RNGSelection(selections, (ValueType*)(vectorInfo->c_str() + m_metaDataSize), p_index, VID, replicaCount, HeadPrev);
            auto selectEnd = std::chrono::high_resolution_clock::now();
            auto elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(selectEnd - selectBegin).count();
            m_stat.m_selectCost += elapsedMSeconds;

            auto reassignAppendBegin = std::chrono::high_resolution_clock::now();
            // LOG(Helper::LogLevel::LL_Info, "Need ReAssign\n");
            if (isNeedReassign && m_versionMap->GetVersion(VID) == version) {
                // LOG(Helper::LogLevel::LL_Info, "Update Version: VID: %d, version: %d, current version: %d\n", VID, version, m_versionMap.GetVersion(VID));
                m_versionMap->IncVersion(VID, &version);
                (*vectorInfo)[sizeof(VID)] = version;

                //LOG(Helper::LogLevel::LL_Info, "Reassign: oldVID:%d, replicaCount:%d, candidateNum:%d, dist0:%f\n", oldVID, replicaCount, i, selections[0].distance);
                for (int i = 0; i < replicaCount && m_versionMap->GetVersion(VID) == version; i++) {
                    //LOG(Helper::LogLevel::LL_Info, "Reassign: headID :%d, oldVID:%d, newVID:%d, posting length: %d, dist: %f, string size: %d\n", headID, oldVID, VID, m_postingSizes[headID].load(), selections[i].distance, newPart.size());
                    if (ErrorCode::Undefined == ConcurrentAppend(p_index, selections[i].node, 1, *vectorInfo, 3)) {
                        // LOG(Helper::LogLevel::LL_Info, "Head Miss: VID: %d, current version: %d, another re-assign\n", VID, version);
                        break;
                    }
                }
            }
            auto reassignAppendEnd = std::chrono::high_resolution_clock::now();
            elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(reassignAppendEnd - reassignAppendBegin).count();
            m_stat.m_reAssignAppendCost += elapsedMSeconds;

            auto reassignEnd = std::chrono::high_resolution_clock::now();
            elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(reassignEnd - reassignBegin).count();
            m_stat.m_reAssignCost += elapsedMSeconds;
        }

        void ConcurrentReassignSingleVector(VectorIndex* p_index, std::shared_ptr<std::string> vectorInfo, SizeType HeadPrev, int replicas)
        {
            SizeType VID = *((SizeType*)vectorInfo->c_str());
            uint8_t version = *((uint8_t*)(vectorInfo->c_str() + sizeof(VID)));
            // return;
            // LOG(Helper::LogLevel::LL_Info, "ReassignID: %d, version: %d, current version: %d, HeadPrev: %d\n", VID, version, m_versionMap->GetVersion(VID), HeadPrev);
            if (m_versionMap->Deleted(VID) || m_versionMap->GetVersion(VID) != version) {
                return;
            }
            auto reassignBegin = std::chrono::high_resolution_clock::now();

            m_stat.m_reAssignNum++;

            auto selectBegin = std::chrono::high_resolution_clock::now();
            std::vector<Edge> selections(replicas);
            int replicaCount;
            bool isNeedReassign = RNGSelection(selections, (ValueType*)(vectorInfo->c_str() + m_metaDataSize), p_index, VID, replicaCount, HeadPrev);
            auto selectEnd = std::chrono::high_resolution_clock::now();
            auto elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(selectEnd - selectBegin).count();
            m_stat.m_selectCost += elapsedMSeconds;

            auto reassignAppendBegin = std::chrono::high_resolution_clock::now();
            // LOG(Helper::LogLevel::LL_Info, "Need ReAssign\n");
            if (isNeedReassign && m_versionMap->GetVersion(VID) == version) {
                // LOG(Helper::LogLevel::LL_Info, "Update Version: VID: %d, version: %d, current version: %d\n", VID, version, m_versionMap.GetVersion(VID));
                m_versionMap->IncVersion(VID, &version);
                (*vectorInfo)[sizeof(VID)] = version;
                
                bool find = false;
                //LOG(Helper::LogLevel::LL_Info, "Reassign: oldVID:%d, replicaCount:%d, candidateNum:%d, dist0:%f\n", oldVID, replicaCount, i, selections[0].distance);
                for (int i = 0; i < replicaCount && m_versionMap->GetVersion(VID) == version; i++) {
                    if(find) break;
                    if(selections[i].node == HeadPrev)
                        continue;

                    std::string postingList;
                    if (db->Get(selections[i].node, &postingList) != ErrorCode::Success) {
                        continue;
                    }
                    
                    //LOG(Helper::LogLevel::LL_Info, "Reassign: headID :%d, oldVID:%d, newVID:%d, posting length: %d, dist: %f, string size: %d\n", headID, oldVID, VID, m_postingSizes[headID].load(), selections[i].distance, newPart.size());
                    if (ErrorCode::Undefined == ConcurrentAppend(p_index, selections[i].node, 1, *vectorInfo, 3)) {
                        // LOG(Helper::LogLevel::LL_Info, "Head Miss: VID: %d, current version: %d, another re-assign\n", VID, version);
                        break;
                    }
                    find = true;
                }
            }
            auto reassignAppendEnd = std::chrono::high_resolution_clock::now();
            elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(reassignAppendEnd - reassignAppendBegin).count();
            m_stat.m_reAssignAppendCost += elapsedMSeconds;

            auto reassignEnd = std::chrono::high_resolution_clock::now();
            elapsedMSeconds = std::chrono::duration_cast<std::chrono::microseconds>(reassignEnd - reassignBegin).count();
            m_stat.m_reAssignCost += elapsedMSeconds;
        }

        bool LoadIndex(Options& p_opt, COMMON::VersionLabel& p_versionMap) override {
            m_versionMap = &p_versionMap;
            m_opt = &p_opt;
            LOG(Helper::LogLevel::LL_Info, "DataBlockSize: %d, Capacity: %d\n", m_opt->m_datasetRowsInBlock, m_opt->m_datasetCapacity);

            if (!m_opt->m_useSPDK) {
                m_versionMap->Load(m_opt->m_deleteIDFile, m_opt->m_datasetRowsInBlock, m_opt->m_datasetCapacity);
                m_postingSizes.Load(m_opt->m_ssdInfoFile, m_opt->m_datasetRowsInBlock, m_opt->m_datasetCapacity);
                LOG(Helper::LogLevel::LL_Info, "Current vector num: %d.\n", m_versionMap->GetVectorNum());
                LOG(Helper::LogLevel::LL_Info, "Current posting num: %d.\n", m_postingSizes.GetPostingNum());

                ShowPostingDistribution(m_opt->m_startNum - m_opt->m_step, true);
            }      

            if (m_opt->m_update) {
                LOG(Helper::LogLevel::LL_Info, "SPFresh: initialize thread pools, append: %d, reassign %d\n", m_opt->m_appendThreadNum, m_opt->m_reassignThreadNum);
                m_splitThreadPool = std::make_shared<SPDKThreadPool>();
                m_splitThreadPool->initSPDK(m_opt->m_appendThreadNum, this);
                m_reassignThreadPool = std::make_shared<SPDKThreadPool>();
                m_reassignThreadPool->initSPDK(m_opt->m_reassignThreadNum, this);
                LOG(Helper::LogLevel::LL_Info, "SPFresh: finish initialization\n");
            }
            return true;
        }

        bool LoadIndex(Options& p_opt, COMMON::VersionLabel& p_versionMap, COMMON::PostingVersionLabel& p_postingVersionMap) override {
            m_versionMap = &p_versionMap;
            m_postingVersionMap = &p_postingVersionMap;
            m_opt = &p_opt;
            LOG(Helper::LogLevel::LL_Info, "DataBlockSize: %d, Capacity: %d\n", m_opt->m_datasetRowsInBlock, m_opt->m_datasetCapacity);

            if (!m_opt->m_useSPDK) {
                m_versionMap->Load(m_opt->m_deleteIDFile, m_opt->m_datasetRowsInBlock, m_opt->m_datasetCapacity);
                m_postingVersionMap->Load(m_opt->m_postingVersionLabelFile, m_opt->m_datasetRowsInBlock, m_opt->m_datasetCapacity);
                m_postingSizes.Load(m_opt->m_ssdInfoFile, m_opt->m_datasetRowsInBlock, m_opt->m_datasetCapacity);
                LOG(Helper::LogLevel::LL_Info, "Current vector num: %d.\n", m_versionMap->GetVectorNum());
                LOG(Helper::LogLevel::LL_Info, "Current posting num: (%d,%d).\n", m_postingVersionMap->GetPostingNum(), m_postingSizes.GetPostingNum());

                ShowPostingDistribution(m_opt->m_startNum - m_opt->m_step, true);
            }      

            if (m_opt->m_update) {
                LOG(Helper::LogLevel::LL_Info, "SPFresh: initialize thread pools, append: %d, reassign %d\n", m_opt->m_appendThreadNum, m_opt->m_reassignThreadNum);
                m_splitThreadPool = std::make_shared<SPDKThreadPool>();
                m_splitThreadPool->initSPDK(m_opt->m_appendThreadNum, this);
                m_reassignThreadPool = std::make_shared<SPDKThreadPool>();
                m_reassignThreadPool->initSPDK(m_opt->m_reassignThreadNum, this);
                LOG(Helper::LogLevel::LL_Info, "SPFresh: finish initialization\n");
            }
            return true;
        }

        virtual void SearchIndex(ExtraWorkSpace* p_exWorkSpace,
            QueryResult& p_queryResults,
            std::shared_ptr<VectorIndex> p_index,
            SearchStats* p_stats, std::set<int>* truth, std::map<int, std::set<int>>* found) override
        {
            auto exStart = std::chrono::high_resolution_clock::now();

            // const auto postingListCount = static_cast<uint32_t>(p_exWorkSpace->m_postingIDs.size());

            p_exWorkSpace->m_deduper.clear();

            auto exSetUpEnd = std::chrono::high_resolution_clock::now();

            p_stats->m_exSetUpLatency = ((double)std::chrono::duration_cast<std::chrono::microseconds>(exSetUpEnd - exStart).count()) / 1000;

            COMMON::QueryResultSet<ValueType>& queryResults = *((COMMON::QueryResultSet<ValueType>*) & p_queryResults);

            int diskRead = 0;
            int diskIO = 0;
            int listElements = 0;

            double compLatency = 0;
            double readLatency = 0;

            std::vector<std::string> postingLists;

            std::chrono::microseconds remainLimit = m_hardLatencyLimit - std::chrono::microseconds((int)p_stats->m_totalLatency);

            auto readStart = std::chrono::high_resolution_clock::now();
            std::vector<SizeType> filtered_postingIDs;
            for(uint32_t i = 0; i < p_exWorkSpace->m_postingIDs.size(); i++){
                SizeType curPostingID = p_exWorkSpace->m_postingIDs[i];

                if(m_postingVersionMap->Deleted(curPostingID) || !p_index->ContainSample(curPostingID) || m_postingSizes.GetSize(curPostingID) <= 0) continue;

                filtered_postingIDs.emplace_back(curPostingID);
            }

            db->MultiGet(filtered_postingIDs, &postingLists, remainLimit);
            auto readEnd = std::chrono::high_resolution_clock::now();

            for (uint32_t pi = 0; pi < postingLists.size(); ++pi) {
                diskIO += ((postingLists[pi].size() + PageSize - 1) >> PageSizeEx);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            readLatency += ((double)std::chrono::duration_cast<std::chrono::microseconds>(readEnd - readStart).count());
            //std::unordered_set<int> searched;
            for (uint32_t pi = 0; pi < postingLists.size(); ++pi) {
                auto curPostingID = p_exWorkSpace->m_postingIDs[pi];
                std::string& postingList = postingLists[pi];

                int vectorNum = (int)(postingList.size() / m_vectorInfoSize);

                int realNum = vectorNum;

                diskRead += (int)(postingList.size());
                listElements += vectorNum;

                auto compStart = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < vectorNum; i++) {
                    char* vectorInfo = postingList.data() + i * m_vectorInfoSize;
                    int vectorID = *(reinterpret_cast<int*>(vectorInfo));
                    //if(searched.count(vectorID) != 0) continue;
                    if (m_versionMap->Deleted(vectorID)) {
                        realNum--;
                        listElements--;
                        continue;
                    }
                    if(p_exWorkSpace->m_deduper.CheckAndSet(vectorID)) {
                        listElements--;
                        continue;
                    }
                    //searched.insert(vectorID);
                    auto distance2leaf = p_index->ComputeDistance(queryResults.GetQuantizedTarget(), vectorInfo + m_metaDataSize);
                    queryResults.AddPoint(vectorID, distance2leaf);
                }

                std::string_view cached_vecs = m_postingVersionMap->getVectorCacheInfo(curPostingID);
                if(!cached_vecs.empty()){
                    search_count.fetch_add(1);
                    const uint8_t* front_addr = reinterpret_cast<const uint8_t*>(&cached_vecs.front());
                    //uint8_t* front_addr = reinterpret_cast<uint8_t*>(&cached_vecs.front());
                    int vec_num = cached_vecs.size() / m_vectorInfoSize;
                    //LOG(Helper::LogLevel::LL_Info, "posting %d, cache vector number:%d\n", curPostingID, vec_num);
                    
            
                    for(int j = 0; j < vec_num; j++){
                        auto t2 = std::chrono::high_resolution_clock::now();
                        if(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() > 6000) break;
                        const uint8_t* vector_info = front_addr + j * m_vectorInfoSize;
                        const SizeType vid = *(reinterpret_cast<const SizeType*>(vector_info));
                        _mm_prefetch(reinterpret_cast<const char*>(vector_info + m_vectorInfoSize), _MM_HINT_T0);
                        // if(vid > m_versionMap->GetVectorNum() || vid < 0 )
                        //     continue;
                        
                        // if(searched.count(vid) != 0){
                        //     continue;
                        // }
                        if(m_versionMap->Deleted(vid)){
                            continue;
                        }
                        //realNum++;
                        if(p_exWorkSpace->m_deduper.CheckAndSet(vid)){
                            continue;
                        }
                        // searched.insert(vid);
                        //listElements++;
                        auto distance = p_index->ComputeDistance(queryResults.GetQuantizedTarget(), vector_info + m_metaDataSize);
                        queryResults.AddPoint(vid, distance);
                      
                    }
                    
                }
                auto compEnd = std::chrono::high_resolution_clock::now();
                //if (realNum <= m_mergeThreshold && !m_opt->m_inPlace) MergeAsync(p_index.get(), curPostingID);
                if (realNum <= m_mergeThreshold && !m_opt->m_inPlace) {
                    //m_postingVersionMap->Merge(curPostingID);
                    ConcurrentMergeAsync(p_index.get(), curPostingID);
                }

                compLatency += ((double)std::chrono::duration_cast<std::chrono::microseconds>(compEnd - compStart).count());

                if (truth) {
                    for (int i = 0; i < vectorNum; ++i) {
                        char* vectorInfo = postingList.data() + i * m_vectorInfoSize;
                        int vectorID = *(reinterpret_cast<int*>(vectorInfo));
                        if (truth->count(vectorID) != 0)
                            (*found)[curPostingID].insert(vectorID);
                    }
                }
            }

            if (p_stats)
            {
                p_stats->m_compLatency = compLatency / 1000;
                p_stats->m_diskReadLatency = readLatency / 1000;
                p_stats->m_totalListElementsCount = listElements;
                p_stats->m_diskIOCount = diskIO;
                p_stats->m_diskAccessCount = diskRead / 1024;
            }
        }

        virtual void SearchIndex(ExtraWorkSpace* p_exWorkSpace,
            QueryResult& p_queryResults,
            std::shared_ptr<VectorIndex> p_index,
            SearchStats* p_stats, 
            int maxVectorId, std::set<int>* truth, std::map<int, std::set<int>>* found) override
        {
            auto exStart = std::chrono::high_resolution_clock::now();

            // const auto postingListCount = static_cast<uint32_t>(p_exWorkSpace->m_postingIDs.size());

            p_exWorkSpace->m_deduper.clear();

            auto exSetUpEnd = std::chrono::high_resolution_clock::now();

            p_stats->m_exSetUpLatency = ((double)std::chrono::duration_cast<std::chrono::microseconds>(exSetUpEnd - exStart).count()) / 1000;

            COMMON::QueryResultSet<ValueType>& queryResults = *((COMMON::QueryResultSet<ValueType>*) & p_queryResults);

            int diskRead = 0;
            int diskIO = 0;
            int listElements = 0;

            double compLatency = 0;
            double readLatency = 0;

            std::vector<std::string> postingLists;

            std::chrono::microseconds remainLimit = m_hardLatencyLimit - std::chrono::microseconds((int)p_stats->m_totalLatency);

            auto readStart = std::chrono::high_resolution_clock::now();           

            std::vector<SizeType> filtered_postingIDs;
            for(uint32_t i = 0; i < p_exWorkSpace->m_postingIDs.size(); i++){
                SizeType curPostingID = p_exWorkSpace->m_postingIDs[i];

                if(m_postingVersionMap->Deleted(curPostingID) || !p_index->ContainSample(curPostingID)) continue;

                filtered_postingIDs.emplace_back(curPostingID);
            }

            db->MultiGet(filtered_postingIDs, &postingLists, remainLimit);
            auto readEnd = std::chrono::high_resolution_clock::now();

            for (uint32_t pi = 0; pi < postingLists.size(); ++pi) {
                diskIO += ((postingLists[pi].size() + PageSize - 1) >> PageSizeEx);
            }

            readLatency += ((double)std::chrono::duration_cast<std::chrono::microseconds>(readEnd - readStart).count());
            for (uint32_t pi = 0; pi < postingLists.size(); ++pi) {
                auto curPostingID = p_exWorkSpace->m_postingIDs[pi];
                
                std::string& postingList = postingLists[pi];

                int vectorNum = (int)(postingList.size() / m_vectorInfoSize);

                int realNum = vectorNum;

                diskRead += (int)(postingList.size());
                listElements += vectorNum;

                auto compStart = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < vectorNum; i++) {
                    char* vectorInfo = postingList.data() + i * m_vectorInfoSize;
                    int vectorID = *(reinterpret_cast<int*>(vectorInfo));
                    if (m_versionMap->Deleted(vectorID)) {
                        realNum--;
                        listElements--;
                        continue;
                    }
                    if(p_exWorkSpace->m_deduper.CheckAndSet(vectorID) || vectorID >= maxVectorId) {
                        listElements--;
                        continue;
                    }
                    auto distance2leaf = p_index->ComputeDistance(queryResults.GetQuantizedTarget(), vectorInfo + m_metaDataSize);
                    queryResults.AddPoint(vectorID, distance2leaf);
                }

                
                std::string cached_vecs;
                if((cached_vecs = m_postingVersionMap->getVectorCacheInfo(curPostingID)) != ""){
                    uint8_t* front_addr = reinterpret_cast<uint8_t*>(&cached_vecs.front());
                    int vec_num = cached_vecs.size() / m_vectorInfoSize;

                    for(int j = 0; j < vec_num; j++){
                        uint8_t* vector_info = front_addr + j * m_vectorInfoSize;
                        SizeType vid = *(reinterpret_cast<SizeType*>(vector_info));

                        if(m_versionMap->Deleted(vid)){
                            continue;
                        }
                        realNum++;
                        if(p_exWorkSpace->m_deduper.CheckAndSet(vid) || vid >= maxVectorId){
                            continue;
                        }
                        listElements++;
                        auto distance = p_index->ComputeDistance(queryResults.GetQuantizedTarget(), vector_info + m_metaDataSize);
                        queryResults.AddPoint(vid, distance);
                      
                    }
                    
                }

                auto compEnd = std::chrono::high_resolution_clock::now();
                
                if (realNum <= m_mergeThreshold && !m_opt->m_inPlace) {
                    //m_postingVersionMap->Merge(curPostingID);
                    ConcurrentMergeAsync(p_index.get(), curPostingID);
                }
                compLatency += ((double)std::chrono::duration_cast<std::chrono::microseconds>(compEnd - compStart).count());

                if (truth) {
                    for (int i = 0; i < vectorNum; ++i) {
                        char* vectorInfo = postingList.data() + i * m_vectorInfoSize;
                        int vectorID = *(reinterpret_cast<int*>(vectorInfo));
                        if (truth->count(vectorID) != 0)
                            (*found)[curPostingID].insert(vectorID);
                    }
                }
            }

            if (p_stats)
            {
                p_stats->m_compLatency = compLatency / 1000;
                p_stats->m_diskReadLatency = readLatency / 1000;
                p_stats->m_totalListElementsCount = listElements;
                p_stats->m_diskIOCount = diskIO;
                p_stats->m_diskAccessCount = diskRead / 1024;
            }
        }

        bool BuildIndex(std::shared_ptr<Helper::VectorSetReader>& p_reader, std::shared_ptr<VectorIndex> p_headIndex, Options& p_opt, COMMON::VersionLabel& p_versionMap, COMMON::PostingVersionLabel& p_postingVersionMap, SizeType upperBound = -1){
            m_versionMap = &p_versionMap;
            m_postingVersionMap = &p_postingVersionMap;
            m_opt = &p_opt;

            int numThreads = m_opt->m_iSSDNumberOfThreads;
            int candidateNum = m_opt->m_internalResultNum;
            std::unordered_set<SizeType> headVectorIDS;//store the closest vector id  of each posting centroid
            if (m_opt->m_headIDFile.empty()) {
                LOG(Helper::LogLevel::LL_Error, "Not found VectorIDTranslate!\n");
                return false;
            }

            if (fileexists((m_opt->m_indexDirectory + FolderSep + m_opt->m_headIDFile).c_str()))
            {
                auto ptr = SPTAG::f_createIO();
                if (ptr == nullptr || !ptr->Initialize((m_opt->m_indexDirectory + FolderSep + m_opt->m_headIDFile).c_str(), std::ios::binary | std::ios::in)) {
                    LOG(Helper::LogLevel::LL_Error, "failed open VectorIDTranslate: %s\n", m_opt->m_headIDFile.c_str());
                    return false;
                }

                std::uint64_t vid;
                while (ptr->ReadBinary(sizeof(vid), reinterpret_cast<char*>(&vid)) == sizeof(vid))
                {
                    headVectorIDS.insert(static_cast<SizeType>(vid));
                }
                LOG(Helper::LogLevel::LL_Info, "Loaded %u Vector IDs\n", static_cast<uint32_t>(headVectorIDS.size()));
            }

            SizeType fullCount = 0;
            {
                auto fullVectors = p_reader->GetVectorSet();
                fullCount = fullVectors->Count();
                m_vectorInfoSize = fullVectors->PerVectorDataSize() + m_metaDataSize;
            }
            if (upperBound > 0) fullCount = upperBound;

            // m_metaDataSize = sizeof(int) + sizeof(uint8_t) + sizeof(float);
            m_metaDataSize = sizeof(int) + sizeof(uint8_t);

            LOG(Helper::LogLevel::LL_Info, "Build SSD Index.\n");

            Selection selections(static_cast<size_t>(fullCount) * m_opt->m_replicaCount, m_opt->m_tmpdir);
            LOG(Helper::LogLevel::LL_Info, "Full vector count:%d Edge bytes:%llu selection size:%zu, capacity size:%zu\n", fullCount, sizeof(Edge), selections.m_selections.size(), selections.m_selections.capacity());
            std::vector<std::atomic_int> replicaCount(fullCount);//each vector replica num
            std::vector<std::atomic_int> postingListSize(p_headIndex->GetNumSamples());//p_headIndex is the cluster-based memory index, the GetNumSamples() return the number of posting
            for (auto& pls : postingListSize) pls = 0;
            std::unordered_set<SizeType> emptySet;
            SizeType batchSize = (fullCount + m_opt->m_batches - 1) / m_opt->m_batches;

            auto t1 = std::chrono::high_resolution_clock::now();
            if (p_opt.m_batches > 1)
            {
                if (selections.SaveBatch() != ErrorCode::Success)
                {
                    return false;
                }
            }
            {
                LOG(Helper::LogLevel::LL_Info, "Preparation done, start candidate searching.\n");
                SizeType sampleSize = m_opt->m_samples;
                std::vector<SizeType> samples(sampleSize, 0);
                for (int i = 0; i < m_opt->m_batches; i++) {
                    SizeType start = i * batchSize;
                    SizeType end = min(start + batchSize, fullCount);
                    auto fullVectors = p_reader->GetVectorSet(start, end);
                    if (m_opt->m_distCalcMethod == DistCalcMethod::Cosine && !p_reader->IsNormalized()) fullVectors->Normalize(m_opt->m_iSSDNumberOfThreads);

                    if (p_opt.m_batches > 1) {
                        if (selections.LoadBatch(static_cast<size_t>(start) * p_opt.m_replicaCount, static_cast<size_t>(end) * p_opt.m_replicaCount) != ErrorCode::Success)
                        {
                            return false;
                        }
                        emptySet.clear();
                        for (auto vid : headVectorIDS) {
                            if (vid >= start && vid < end) emptySet.insert(vid - start);
                        }
                    }
                    else {
                        emptySet = headVectorIDS;
                    }

                    int sampleNum = 0;
                    for (int j = start; j < end && sampleNum < sampleSize; j++)
                    {
                        if (headVectorIDS.count(j) == 0) samples[sampleNum++] = j - start;
                    }

                    float acc = 0;

                    acc = acc / sampleNum;
                    LOG(Helper::LogLevel::LL_Info, "Batch %d vector(%d,%d) loaded with %d vectors (%zu) HeadIndex acc @%d:%f.\n", i, start, end, fullVectors->Count(), selections.m_selections.size(), candidateNum, acc);

                    p_headIndex->ApproximateRNG(fullVectors, emptySet, candidateNum, selections.m_selections.data(), m_opt->m_replicaCount, numThreads, m_opt->m_gpuSSDNumTrees, m_opt->m_gpuSSDLeafSize, m_opt->m_rngFactor, m_opt->m_numGPUs);
                    LOG(Helper::LogLevel::LL_Info, "Batch %d finished!\n", i);

                    for (SizeType j = start; j < end; j++) {
                        replicaCount[j] = 0;
                        size_t vecOffset = j * (size_t)m_opt->m_replicaCount;//every m_opt->m_replicaCount vectors in selections are the replicas of a vector
                        if (headVectorIDS.count(j) == 0) {//if vector j is not the centroid vector, calculate the postingSize
                            for (int resNum = 0; resNum < m_opt->m_replicaCount && selections[vecOffset + resNum].node != INT_MAX; resNum++) {
                                ++postingListSize[selections[vecOffset + resNum].node];
                                selections[vecOffset + resNum].tonode = j;
                                ++replicaCount[j];
                            }
                        }
                    }

                    if (p_opt.m_batches > 1)
                    {
                        if (selections.SaveBatch() != ErrorCode::Success)
                        {
                            return false;
                        }
                    }
                }
            }
            int total = 0;
            for(int i = 0; i < postingListSize.size(); i++){
                //std::cout<<"("<<i<<","<<m_postingSizes.GetSize(i)<<")"<<std::endl;
                total += postingListSize[i].load();
                
            }

            LOG(Helper::LogLevel::LL_Info, "current memory vector num:%d\n", total);

            auto t2 = std::chrono::high_resolution_clock::now();
            LOG(Helper::LogLevel::LL_Info, "Searching replicas ended. Search Time: %.2lf mins\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count()) / 60.0);

            if (p_opt.m_batches > 1)
            {
                if (selections.LoadBatch(0, static_cast<size_t>(fullCount) * p_opt.m_replicaCount) != ErrorCode::Success)
                {
                    return false;
                }
            }

            // Sort results either in CPU or GPU
            VectorIndex::SortSelections(&selections.m_selections);

            auto t3 = std::chrono::high_resolution_clock::now();
            LOG(Helper::LogLevel::LL_Info, "Time to sort selections:%.2lf sec.\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t3 - t2).count()) + ((double)std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()) / 1000);

            auto postingSizeLimit = m_postingSizeLimit;
            if (m_opt->m_postingPageLimit > 0)
            {
                postingSizeLimit = static_cast<int>(m_opt->m_postingPageLimit * PageSize / m_vectorInfoSize);
            }

            LOG(Helper::LogLevel::LL_Info, "Posting size limit: %d\n", postingSizeLimit);


            {
                std::vector<int> replicaCountDist(m_opt->m_replicaCount + 1, 0);
                for (int i = 0; i < replicaCount.size(); ++i)
                {
                    if (headVectorIDS.count(i) > 0) continue;
                    ++replicaCountDist[replicaCount[i]];
                }

                LOG(Helper::LogLevel::LL_Info, "Before Posting Cut:\n");
                for (int i = 0; i < replicaCountDist.size(); ++i)
                {
                    LOG(Helper::LogLevel::LL_Info, "Replica Count Dist: %d, %d\n", i, replicaCountDist[i]);
                }
            }

    #pragma omp parallel for schedule(dynamic)
            for (int i = 0; i < postingListSize.size(); ++i)
            {
                if (postingListSize[i] <= postingSizeLimit) continue;

                std::size_t selectIdx = std::lower_bound(selections.m_selections.begin(), selections.m_selections.end(), i, Selection::g_edgeComparer) - selections.m_selections.begin();

                for (size_t dropID = postingSizeLimit; dropID < postingListSize[i]; ++dropID)
                {
                    int tonode = selections.m_selections[selectIdx + dropID].tonode;//vector id
                    --replicaCount[tonode];
                }
                postingListSize[i] = postingSizeLimit;
            }
            {
                std::vector<int> replicaCountDist(m_opt->m_replicaCount + 1, 0);
                for (int i = 0; i < replicaCount.size(); ++i)
                {
                    ++replicaCountDist[replicaCount[i]];
                }

                LOG(Helper::LogLevel::LL_Info, "After Posting Cut:\n");
                for (int i = 0; i < replicaCountDist.size(); ++i)
                {
                    LOG(Helper::LogLevel::LL_Info, "Replica Count Dist: %d, %d\n", i, replicaCountDist[i]);
                }
            }

            auto t4 = std::chrono::high_resolution_clock::now();
            LOG(SPTAG::Helper::LogLevel::LL_Info, "Time to perform posting cut:%.2lf sec.\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t4 - t3).count()) + ((double)std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()) / 1000);

            auto fullVectors = p_reader->GetVectorSet();
            if (m_opt->m_distCalcMethod == DistCalcMethod::Cosine && !p_reader->IsNormalized()) fullVectors->Normalize(m_opt->m_iSSDNumberOfThreads);

            LOG(Helper::LogLevel::LL_Info, "SPFresh: initialize versionMap\n");
            m_versionMap->Initialize(fullCount, p_headIndex->m_iDataBlockSize, p_headIndex->m_iDataCapacity);
            m_postingVersionMap->Initialize(postingListSize.size(), p_headIndex->m_iDataBlockSize, p_headIndex->m_iDataCapacity);

            LOG(Helper::LogLevel::LL_Info, "SPFresh: Writing values to DB\n");

            std::vector<int> postingListSize_int(postingListSize.begin(), postingListSize.end());

            WriteDownAllPostingToDB(postingListSize_int, selections, fullVectors);

            m_postingSizes.Initialize((SizeType)(postingListSize.size()), p_headIndex->m_iDataBlockSize, p_headIndex->m_iDataCapacity);
            for (int i = 0; i < postingListSize.size(); i++) {
                m_postingSizes.UpdateSize(i, postingListSize[i]);
            }

            SavePostingSizesAndVersionMaps();

            auto t5 = std::chrono::high_resolution_clock::now();
            double elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(t5 - t1).count();
            LOG(Helper::LogLevel::LL_Info, "Total used time: %.2lf minutes (about %.2lf hours).\n", elapsedSeconds / 60.0, elapsedSeconds / 3600.0);
            return true;
        }


        bool BuildIndex(std::shared_ptr<Helper::VectorSetReader>& p_reader, SizeType v_begin, SizeType v_end, std::shared_ptr<VectorIndex> p_headIndex, Options& p_opt, COMMON::VersionLabel& p_versionMap, COMMON::PostingVersionLabel& p_postingVersionMap, SizeType upperBound = -1){
            m_versionMap = &p_versionMap;
            m_postingVersionMap = &p_postingVersionMap;
            m_opt = &p_opt;

            int numThreads = m_opt->m_iSSDNumberOfThreads;
            int candidateNum = m_opt->m_internalResultNum;
            std::unordered_set<SizeType> headVectorIDS;//store the closest vector id  of each posting centroid
            if (m_opt->m_headIDFile.empty()) {
                LOG(Helper::LogLevel::LL_Error, "Not found VectorIDTranslate!\n");
                return false;
            }

            if (fileexists((m_opt->m_indexDirectory + FolderSep + m_opt->m_headIDFile).c_str()))
            {
                auto ptr = SPTAG::f_createIO();
                if (ptr == nullptr || !ptr->Initialize((m_opt->m_indexDirectory + FolderSep + m_opt->m_headIDFile).c_str(), std::ios::binary | std::ios::in)) {
                    LOG(Helper::LogLevel::LL_Error, "failed open VectorIDTranslate: %s\n", m_opt->m_headIDFile.c_str());
                    return false;
                }

                std::uint64_t vid;
                while (ptr->ReadBinary(sizeof(vid), reinterpret_cast<char*>(&vid)) == sizeof(vid))
                {
                    headVectorIDS.insert(static_cast<SizeType>(vid));
                }
                LOG(Helper::LogLevel::LL_Info, "Loaded %u Vector IDs\n", static_cast<uint32_t>(headVectorIDS.size()));
            }

            SizeType fullCount = 0;
            {
                auto fullVectors = p_reader->GetVectorSet(v_begin, v_end);
                fullCount = fullVectors->Count();
                m_vectorInfoSize = fullVectors->PerVectorDataSize() + m_metaDataSize;
            }
            if (upperBound > 0) fullCount = upperBound;

            // m_metaDataSize = sizeof(int) + sizeof(uint8_t) + sizeof(float);
            m_metaDataSize = sizeof(int) + sizeof(uint8_t);

            LOG(Helper::LogLevel::LL_Info, "Build SSD Index.\n");

            Selection selections(static_cast<size_t>(fullCount) * m_opt->m_replicaCount, m_opt->m_tmpdir);
            LOG(Helper::LogLevel::LL_Info, "Full vector count:%d Edge bytes:%llu selection size:%zu, capacity size:%zu\n", fullCount, sizeof(Edge), selections.m_selections.size(), selections.m_selections.capacity());
            std::vector<std::atomic_int> replicaCount(fullCount);//each vector replica num
            std::vector<std::atomic_int> postingListSize(p_headIndex->GetNumSamples());//p_headIndex is the cluster-based memory index, the GetNumSamples() return the number of posting
            for (auto& pls : postingListSize) pls = 0;
            std::unordered_set<SizeType> emptySet;
            SizeType batchSize = (fullCount + m_opt->m_batches - 1) / m_opt->m_batches;

            auto t1 = std::chrono::high_resolution_clock::now();
            if (p_opt.m_batches > 1)
            {
                if (selections.SaveBatch() != ErrorCode::Success)
                {
                    return false;
                }
            }
            {
                LOG(Helper::LogLevel::LL_Info, "Preparation done, start candidate searching.\n");
                SizeType sampleSize = m_opt->m_samples;
                std::vector<SizeType> samples(sampleSize, 0);
                for (int i = 0; i < m_opt->m_batches; i++) {
                    SizeType start = i * batchSize;
                    SizeType end = min(start + batchSize, fullCount);
                    auto fullVectors = p_reader->GetVectorSet(start, end);
                    if (m_opt->m_distCalcMethod == DistCalcMethod::Cosine && !p_reader->IsNormalized()) fullVectors->Normalize(m_opt->m_iSSDNumberOfThreads);

                    if (p_opt.m_batches > 1) {
                        if (selections.LoadBatch(static_cast<size_t>(start) * p_opt.m_replicaCount, static_cast<size_t>(end) * p_opt.m_replicaCount) != ErrorCode::Success)
                        {
                            return false;
                        }
                        emptySet.clear();
                        for (auto vid : headVectorIDS) {
                            if (vid >= start && vid < end) emptySet.insert(vid - start);
                        }
                    }
                    else {
                        emptySet = headVectorIDS;
                    }

                    int sampleNum = 0;
                    for (int j = start; j < end && sampleNum < sampleSize; j++)
                    {
                        if (headVectorIDS.count(j) == 0) samples[sampleNum++] = j - start;
                    }

                    float acc = 0;

                    acc = acc / sampleNum;
                    LOG(Helper::LogLevel::LL_Info, "Batch %d vector(%d,%d) loaded with %d vectors (%zu) HeadIndex acc @%d:%f.\n", i, start, end, fullVectors->Count(), selections.m_selections.size(), candidateNum, acc);

                    p_headIndex->ApproximateRNG(fullVectors, emptySet, candidateNum, selections.m_selections.data(), m_opt->m_replicaCount, numThreads, m_opt->m_gpuSSDNumTrees, m_opt->m_gpuSSDLeafSize, m_opt->m_rngFactor, m_opt->m_numGPUs);
                    LOG(Helper::LogLevel::LL_Info, "Batch %d finished!\n", i);

                    for (SizeType j = start; j < end; j++) {
                        replicaCount[j] = 0;
                        size_t vecOffset = j * (size_t)m_opt->m_replicaCount;//every m_opt->m_replicaCount vectors in selections are the replicas of a vector
                        if (headVectorIDS.count(j) == 0) {//if vector j is not the centroid vector, calculate the postingSize
                            for (int resNum = 0; resNum < m_opt->m_replicaCount && selections[vecOffset + resNum].node != INT_MAX; resNum++) {
                                ++postingListSize[selections[vecOffset + resNum].node];
                                selections[vecOffset + resNum].tonode = j;
                                ++replicaCount[j];
                            }
                        }
                    }

                    if (p_opt.m_batches > 1)
                    {
                        if (selections.SaveBatch() != ErrorCode::Success)
                        {
                            return false;
                        }
                    }
                }
            }
            auto t2 = std::chrono::high_resolution_clock::now();
            LOG(Helper::LogLevel::LL_Info, "Searching replicas ended. Search Time: %.2lf mins\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count()) / 60.0);

            if (p_opt.m_batches > 1)
            {
                if (selections.LoadBatch(0, static_cast<size_t>(fullCount) * p_opt.m_replicaCount) != ErrorCode::Success)
                {
                    return false;
                }
            }

            // Sort results either in CPU or GPU
            VectorIndex::SortSelections(&selections.m_selections);

            auto t3 = std::chrono::high_resolution_clock::now();
            LOG(Helper::LogLevel::LL_Info, "Time to sort selections:%.2lf sec.\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t3 - t2).count()) + ((double)std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()) / 1000);

            auto postingSizeLimit = m_postingSizeLimit;
            if (m_opt->m_postingPageLimit > 0)
            {
                postingSizeLimit = static_cast<int>(m_opt->m_postingPageLimit * PageSize / m_vectorInfoSize);
            }

            LOG(Helper::LogLevel::LL_Info, "Posting size limit: %d\n", postingSizeLimit);


            {
                std::vector<int> replicaCountDist(m_opt->m_replicaCount + 1, 0);
                for (int i = 0; i < replicaCount.size(); ++i)
                {
                    if (headVectorIDS.count(i) > 0) continue;
                    ++replicaCountDist[replicaCount[i]];
                }

                LOG(Helper::LogLevel::LL_Info, "Before Posting Cut:\n");
                for (int i = 0; i < replicaCountDist.size(); ++i)
                {
                    LOG(Helper::LogLevel::LL_Info, "Replica Count Dist: %d, %d\n", i, replicaCountDist[i]);
                }
            }

    #pragma omp parallel for schedule(dynamic)
            for (int i = 0; i < postingListSize.size(); ++i)
            {
                if (postingListSize[i] <= postingSizeLimit) continue;

                std::size_t selectIdx = std::lower_bound(selections.m_selections.begin(), selections.m_selections.end(), i, Selection::g_edgeComparer) - selections.m_selections.begin();

                for (size_t dropID = postingSizeLimit; dropID < postingListSize[i]; ++dropID)
                {
                    int tonode = selections.m_selections[selectIdx + dropID].tonode;//vector id
                    --replicaCount[tonode];
                }
                postingListSize[i] = postingSizeLimit;
            }
            {
                std::vector<int> replicaCountDist(m_opt->m_replicaCount + 1, 0);
                for (int i = 0; i < replicaCount.size(); ++i)
                {
                    ++replicaCountDist[replicaCount[i]];
                }

                LOG(Helper::LogLevel::LL_Info, "After Posting Cut:\n");
                for (int i = 0; i < replicaCountDist.size(); ++i)
                {
                    LOG(Helper::LogLevel::LL_Info, "Replica Count Dist: %d, %d\n", i, replicaCountDist[i]);
                }
            }

            auto t4 = std::chrono::high_resolution_clock::now();
            LOG(SPTAG::Helper::LogLevel::LL_Info, "Time to perform posting cut:%.2lf sec.\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t4 - t3).count()) + ((double)std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()) / 1000);

            auto fullVectors = p_reader->GetVectorSet(v_begin, v_end);
            if (m_opt->m_distCalcMethod == DistCalcMethod::Cosine && !p_reader->IsNormalized()) fullVectors->Normalize(m_opt->m_iSSDNumberOfThreads);

            LOG(Helper::LogLevel::LL_Info, "SPFresh: initialize versionMap\n");
            m_versionMap->Initialize(fullCount, p_headIndex->m_iDataBlockSize, p_headIndex->m_iDataCapacity);
            m_postingVersionMap->Initialize(postingListSize.size(), p_headIndex->m_iDataBlockSize, p_headIndex->m_iDataCapacity);

            LOG(Helper::LogLevel::LL_Info, "SPFresh: Writing values to DB\n");

            std::vector<int> postingListSize_int(postingListSize.begin(), postingListSize.end());

            WriteDownAllPostingToDB(postingListSize_int, selections, fullVectors);

            m_postingSizes.Initialize((SizeType)(postingListSize.size()), p_headIndex->m_iDataBlockSize, p_headIndex->m_iDataCapacity);
            for (int i = 0; i < postingListSize.size(); i++) {
                m_postingSizes.UpdateSize(i, postingListSize[i]);
            }

            SavePostingSizesAndVersionMaps();

            auto t5 = std::chrono::high_resolution_clock::now();
            double elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(t5 - t1).count();
            LOG(Helper::LogLevel::LL_Info, "Total used time: %.2lf minutes (about %.2lf hours).\n", elapsedSeconds / 60.0, elapsedSeconds / 3600.0);
            return true;
        }

        bool BuildIndex(std::shared_ptr<Helper::VectorSetReader>& p_reader, std::shared_ptr<VectorIndex> p_headIndex, Options& p_opt, COMMON::VersionLabel& p_versionMap, SizeType upperBound = -1) override {
            m_versionMap = &p_versionMap;
            m_opt = &p_opt;

            int numThreads = m_opt->m_iSSDNumberOfThreads;
            int candidateNum = m_opt->m_internalResultNum;
            std::unordered_set<SizeType> headVectorIDS;//store the closest vector id  of each posting centroid
            if (m_opt->m_headIDFile.empty()) {
                LOG(Helper::LogLevel::LL_Error, "Not found VectorIDTranslate!\n");
                return false;
            }

            if (fileexists((m_opt->m_indexDirectory + FolderSep + m_opt->m_headIDFile).c_str()))
            {
                auto ptr = SPTAG::f_createIO();
                if (ptr == nullptr || !ptr->Initialize((m_opt->m_indexDirectory + FolderSep + m_opt->m_headIDFile).c_str(), std::ios::binary | std::ios::in)) {
                    LOG(Helper::LogLevel::LL_Error, "failed open VectorIDTranslate: %s\n", m_opt->m_headIDFile.c_str());
                    return false;
                }

                std::uint64_t vid;
                while (ptr->ReadBinary(sizeof(vid), reinterpret_cast<char*>(&vid)) == sizeof(vid))
                {
                    headVectorIDS.insert(static_cast<SizeType>(vid));
                }
                LOG(Helper::LogLevel::LL_Info, "Loaded %u Vector IDs\n", static_cast<uint32_t>(headVectorIDS.size()));
            }

            SizeType fullCount = 0;
            {
                auto fullVectors = p_reader->GetVectorSet();
                fullCount = fullVectors->Count();
                m_vectorInfoSize = fullVectors->PerVectorDataSize() + m_metaDataSize;
            }
            if (upperBound > 0) fullCount = upperBound;

            // m_metaDataSize = sizeof(int) + sizeof(uint8_t) + sizeof(float);
            m_metaDataSize = sizeof(int) + sizeof(uint8_t);

            LOG(Helper::LogLevel::LL_Info, "Build SSD Index.\n");

            Selection selections(static_cast<size_t>(fullCount) * m_opt->m_replicaCount, m_opt->m_tmpdir);
            LOG(Helper::LogLevel::LL_Info, "Full vector count:%d Edge bytes:%llu selection size:%zu, capacity size:%zu\n", fullCount, sizeof(Edge), selections.m_selections.size(), selections.m_selections.capacity());
            std::vector<std::atomic_int> replicaCount(fullCount);//each vector replica num
            std::vector<std::atomic_int> postingListSize(p_headIndex->GetNumSamples());//p_headIndex is the cluster-based memory index, the GetNumSamples() return the number of posting
            for (auto& pls : postingListSize) pls = 0;
            std::unordered_set<SizeType> emptySet;
            SizeType batchSize = (fullCount + m_opt->m_batches - 1) / m_opt->m_batches;

            auto t1 = std::chrono::high_resolution_clock::now();
            if (p_opt.m_batches > 1)
            {
                if (selections.SaveBatch() != ErrorCode::Success)
                {
                    return false;
                }
            }
            {
                LOG(Helper::LogLevel::LL_Info, "Preparation done, start candidate searching.\n");
                SizeType sampleSize = m_opt->m_samples;
                std::vector<SizeType> samples(sampleSize, 0);
                for (int i = 0; i < m_opt->m_batches; i++) {
                    SizeType start = i * batchSize;
                    SizeType end = min(start + batchSize, fullCount);
                    auto fullVectors = p_reader->GetVectorSet(start, end);
                    if (m_opt->m_distCalcMethod == DistCalcMethod::Cosine && !p_reader->IsNormalized()) fullVectors->Normalize(m_opt->m_iSSDNumberOfThreads);

                    if (p_opt.m_batches > 1) {
                        if (selections.LoadBatch(static_cast<size_t>(start) * p_opt.m_replicaCount, static_cast<size_t>(end) * p_opt.m_replicaCount) != ErrorCode::Success)
                        {
                            return false;
                        }
                        emptySet.clear();
                        for (auto vid : headVectorIDS) {
                            if (vid >= start && vid < end) emptySet.insert(vid - start);
                        }
                    }
                    else {
                        emptySet = headVectorIDS;
                    }

                    int sampleNum = 0;
                    for (int j = start; j < end && sampleNum < sampleSize; j++)
                    {
                        if (headVectorIDS.count(j) == 0) samples[sampleNum++] = j - start;
                    }

                    float acc = 0;
// #pragma omp parallel for schedule(dynamic)
//                     for (int j = 0; j < sampleNum; j++)
//                     {
//                         COMMON::Utils::atomic_float_add(&acc, COMMON::TruthSet::CalculateRecall(p_headIndex.get(), fullVectors->GetVector(samples[j]), candidateNum));
//                     }
                    acc = acc / sampleNum;
                    LOG(Helper::LogLevel::LL_Info, "Batch %d vector(%d,%d) loaded with %d vectors (%zu) HeadIndex acc @%d:%f.\n", i, start, end, fullVectors->Count(), selections.m_selections.size(), candidateNum, acc);

                    p_headIndex->ApproximateRNG(fullVectors, emptySet, candidateNum, selections.m_selections.data(), m_opt->m_replicaCount, numThreads, m_opt->m_gpuSSDNumTrees, m_opt->m_gpuSSDLeafSize, m_opt->m_rngFactor, m_opt->m_numGPUs);
                    LOG(Helper::LogLevel::LL_Info, "Batch %d finished!\n", i);

                    for (SizeType j = start; j < end; j++) {
                        replicaCount[j] = 0;
                        size_t vecOffset = j * (size_t)m_opt->m_replicaCount;//every m_opt->m_replicaCount vectors in selections are the replicas of a vector
                        if (headVectorIDS.count(j) == 0) {//if vector j is not the centroid vector, calculate the postingSize
                            for (int resNum = 0; resNum < m_opt->m_replicaCount && selections[vecOffset + resNum].node != INT_MAX; resNum++) {
                                ++postingListSize[selections[vecOffset + resNum].node];
                                selections[vecOffset + resNum].tonode = j;
                                ++replicaCount[j];
                            }
                        }
                    }

                    if (p_opt.m_batches > 1)
                    {
                        if (selections.SaveBatch() != ErrorCode::Success)
                        {
                            return false;
                        }
                    }
                }
            }
            auto t2 = std::chrono::high_resolution_clock::now();
            LOG(Helper::LogLevel::LL_Info, "Searching replicas ended. Search Time: %.2lf mins\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count()) / 60.0);

            if (p_opt.m_batches > 1)
            {
                if (selections.LoadBatch(0, static_cast<size_t>(fullCount) * p_opt.m_replicaCount) != ErrorCode::Success)
                {
                    return false;
                }
            }

            // Sort results either in CPU or GPU
            VectorIndex::SortSelections(&selections.m_selections);

            auto t3 = std::chrono::high_resolution_clock::now();
            LOG(Helper::LogLevel::LL_Info, "Time to sort selections:%.2lf sec.\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t3 - t2).count()) + ((double)std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()) / 1000);

            auto postingSizeLimit = m_postingSizeLimit;
            if (m_opt->m_postingPageLimit > 0)
            {
                postingSizeLimit = static_cast<int>(m_opt->m_postingPageLimit * PageSize / m_vectorInfoSize);
            }

            LOG(Helper::LogLevel::LL_Info, "Posting size limit: %d\n", postingSizeLimit);


            {
                std::vector<int> replicaCountDist(m_opt->m_replicaCount + 1, 0);
                for (int i = 0; i < replicaCount.size(); ++i)
                {
                    if (headVectorIDS.count(i) > 0) continue;
                    ++replicaCountDist[replicaCount[i]];
                }

                LOG(Helper::LogLevel::LL_Info, "Before Posting Cut:\n");
                for (int i = 0; i < replicaCountDist.size(); ++i)
                {
                    LOG(Helper::LogLevel::LL_Info, "Replica Count Dist: %d, %d\n", i, replicaCountDist[i]);
                }
            }

    #pragma omp parallel for schedule(dynamic)
            for (int i = 0; i < postingListSize.size(); ++i)
            {
                if (postingListSize[i] <= postingSizeLimit) continue;

                std::size_t selectIdx = std::lower_bound(selections.m_selections.begin(), selections.m_selections.end(), i, Selection::g_edgeComparer) - selections.m_selections.begin();

                for (size_t dropID = postingSizeLimit; dropID < postingListSize[i]; ++dropID)
                {
                    int tonode = selections.m_selections[selectIdx + dropID].tonode;//vector id
                    --replicaCount[tonode];
                }
                postingListSize[i] = postingSizeLimit;
            }
            {
                std::vector<int> replicaCountDist(m_opt->m_replicaCount + 1, 0);
                for (int i = 0; i < replicaCount.size(); ++i)
                {
                    ++replicaCountDist[replicaCount[i]];
                }

                LOG(Helper::LogLevel::LL_Info, "After Posting Cut:\n");
                for (int i = 0; i < replicaCountDist.size(); ++i)
                {
                    LOG(Helper::LogLevel::LL_Info, "Replica Count Dist: %d, %d\n", i, replicaCountDist[i]);
                }
            }

    //         if (m_opt->m_outputEmptyReplicaID)
    //         {
    //             std::vector<int> replicaCountDist(m_opt->m_replicaCount + 1, 0);
    //             auto ptr = SPTAG::f_createIO();
    //             if (ptr == nullptr || !ptr->Initialize("EmptyReplicaID.bin", std::ios::binary | std::ios::out)) {
    //                 LOG(Helper::LogLevel::LL_Error, "Fail to create EmptyReplicaID.bin!\n");
    //                 return false;
    //             }
    //             for (int i = 0; i < replicaCount.size(); ++i)
    //             {
    //                 if (headVectorIDS.count(i) > 0) continue;

    //                 ++replicaCountDist[replicaCount[i]];

    //                 if (replicaCount[i] < 2)
    //                 {
    //                     long long vid = i;
    //                     if (ptr->WriteBinary(sizeof(vid), reinterpret_cast<char*>(&vid)) != sizeof(vid)) {
    //                         LOG(Helper::LogLevel::LL_Error, "Failt to write EmptyReplicaID.bin!");
    //                         return false;
    //                     }
    //                 }
    //             }

    //             LOG(Helper::LogLevel::LL_Info, "After Posting Cut:\n");
    //             for (int i = 0; i < replicaCountDist.size(); ++i)
    //             {
    //                 LOG(Helper::LogLevel::LL_Info, "Replica Count Dist: %d, %d\n", i, replicaCountDist[i]);
    //             }
    //         }


            auto t4 = std::chrono::high_resolution_clock::now();
            LOG(SPTAG::Helper::LogLevel::LL_Info, "Time to perform posting cut:%.2lf sec.\n", ((double)std::chrono::duration_cast<std::chrono::seconds>(t4 - t3).count()) + ((double)std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()) / 1000);

            auto fullVectors = p_reader->GetVectorSet();
            if (m_opt->m_distCalcMethod == DistCalcMethod::Cosine && !p_reader->IsNormalized()) fullVectors->Normalize(m_opt->m_iSSDNumberOfThreads);

            LOG(Helper::LogLevel::LL_Info, "SPFresh: initialize versionMap\n");
            m_versionMap->Initialize(fullCount, p_headIndex->m_iDataBlockSize, p_headIndex->m_iDataCapacity);

            LOG(Helper::LogLevel::LL_Info, "SPFresh: Writing values to DB\n");

            std::vector<int> postingListSize_int(postingListSize.begin(), postingListSize.end());

            WriteDownAllPostingToDB(postingListSize_int, selections, fullVectors);

            m_postingSizes.Initialize((SizeType)(postingListSize.size()), p_headIndex->m_iDataBlockSize, p_headIndex->m_iDataCapacity);
            for (int i = 0; i < postingListSize.size(); i++) {
                m_postingSizes.UpdateSize(i, postingListSize[i]);
            }

            SavePostingSizesAndVersionMaps();

            auto t5 = std::chrono::high_resolution_clock::now();
            double elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(t5 - t1).count();
            LOG(Helper::LogLevel::LL_Info, "Total used time: %.2lf minutes (about %.2lf hours).\n", elapsedSeconds / 60.0, elapsedSeconds / 3600.0);
            return true;
        }

        void SavePostingSizesAndVersionMaps(){
            LOG(Helper::LogLevel::LL_Info, "SPFresh: Writing SSD Info\n");
            m_postingSizes.Save(m_opt->m_ssdInfoFile);//ssdInfoFile stores the memory postings' ids and sizes
            LOG(Helper::LogLevel::LL_Info, "SPFresh: save versionMap\n");
            m_versionMap->Save(m_opt->m_deleteIDFile);//deleteIDFile stores the versionMap structure
            m_postingVersionMap->Save(m_opt->m_postingVersionLabelFile);
        }

        void WriteDownAllPostingToDB(const std::vector<int>& p_postingListSizes, Selection& p_postingSelections, std::shared_ptr<VectorSet> p_fullVectors) {
    // #pragma omp parallel for num_threads(10)
            std::vector<std::thread> threads;
            std::atomic_size_t vectorsSent(0);
            auto func = [&]()
            {
                Initialize();
                size_t index = 0;
                while (true)
                {
                    index = vectorsSent.fetch_add(1);
                    if (index < p_postingListSizes.size()) {
                        std::string postinglist(m_vectorInfoSize * p_postingListSizes[index], '\0');
                        char* ptr = (char*)postinglist.c_str();
                        std::size_t selectIdx = p_postingSelections.lower_bound(index);
                        for (int j = 0; j < p_postingListSizes[index]; ++j) {
                            if (p_postingSelections[selectIdx].node != index) {
                                LOG(Helper::LogLevel::LL_Error, "Selection ID NOT MATCH\n");
                                exit(1);
                            }
                            SizeType fullID = p_postingSelections[selectIdx++].tonode;
                            // if (id == 0) LOG(Helper::LogLevel::LL_Info, "ID: %d\n", fullID);
                            uint8_t version = m_versionMap->GetVersion(fullID);
                            // First Vector ID, then version, then Vector
                            Serialize(ptr, fullID, version, p_fullVectors->GetVector(fullID));
                            ptr += m_vectorInfoSize;
                        }
                        db->Put(index, postinglist);
                    }
                    else
                    {
                        ExitBlockController();
                        return;
                    }
                }
            };

            for (int j = 0; j < 20; j++) { threads.emplace_back(func); }
            for (auto& thread : threads) { thread.join(); }
        }

        ErrorCode AddIndex(std::shared_ptr<VectorSet>& p_vectorSet,
            std::shared_ptr<VectorIndex> p_index, SizeType begin) override {

            for (int v = 0; v < p_vectorSet->Count(); v++) {
                SizeType VID = begin + v;
                //LOG(Helper::LogLevel::LL_Info, "VID: %d\n",VID);
                std::vector<Edge> selections(static_cast<size_t>(m_opt->m_replicaCount));
                int replicaCount;
                RNGSelection(selections, (ValueType*)(p_vectorSet->GetVector(v)), p_index.get(), VID, replicaCount);

                uint8_t version = m_versionMap->GetVersion(VID);
                std::string appendPosting(m_vectorInfoSize, '\0');
                Serialize((char*)(appendPosting.c_str()), VID, version, p_vectorSet->GetVector(v));//copy VID, version and raw vector data to variable appendPosting
                for (int i = 0; i < replicaCount; i++)
                {
                    //Append(p_index.get(), selections[i].node, 1, appendPosting);
                    
                    //new codes
                    ConcurrentAppend(p_index.get(), selections[i].node, 1, appendPosting);
                    
                }
            }
            return ErrorCode::Success;
        }

        inline void PostingHandle(VectorIndex* p_index, SizeType postingID, int num, std::string vectorInfos, bool reassign = false){

            if(m_postingVersionMap->Deleted(postingID)){
                delete_branch++;
            }
            else if(m_postingVersionMap->IsNormal(postingID)){
                normal_branch++;
                /*if(reassign){
                    uint8_t* addr = reinterpret_cast<uint8_t*>(&vectorInfos.front());

                    for(int i = 0; i < num; i++){
                        //std::string single_vector(m_vectorInfoSize, '\0');
                        //char* single_p = (char*)(single_vector.c_str());
                        //memcpy(single_p, addr + i * m_vectorInfoSize, m_vectorInfoSize);
                        ConcurrentReassignAsync(p_index, std::make_shared<std::string>((char*)(addr + i * m_vectorInfoSize), m_vectorInfoSize), postingID);
                        //ConcurrentReassignSingleVector(p_index, std::make_shared<std::string>(addr + i * m_vectorInfoSize, m_vectorInfoSize), postingID);
                    }
                    
                }
                else{
                    ConcurrentAppend(p_index, postingID, num, vectorInfos);//append num vector data vectorInfos to posting postingID in memory cluster-based index p_index
                }*/
                ConcurrentAppend(p_index, postingID, num, vectorInfos);//append num vector data vectorInfos to posting postingID in memory cluster-based index p_index
            }
            else {//p_index->GetSample(headID) can get the centroid point of posting headID
                split_merge_branch++;
                m_postingVersionMap->addToVectorCache(postingID, num, vectorInfos);
            }
        }

        SizeType SearchVector(std::shared_ptr<VectorSet>& p_vectorSet,
            std::shared_ptr<VectorIndex> p_index, int testNum = 64, SizeType VID = -1) override {
            
            QueryResult queryResults(p_vectorSet->GetVector(0), testNum, false);
            p_index->SearchIndex(queryResults);
            
            std::set<SizeType> checked;
            std::string postingList;
            for (int i = 0; i < queryResults.GetResultNum(); ++i)
            {
                db->Get(queryResults.GetResult(i)->VID, &postingList);
                int vectorNum = (int)(postingList.size() / m_vectorInfoSize);

                for (int j = 0; j < vectorNum; j++) {
                    char* vectorInfo = postingList.data() + j * m_vectorInfoSize;
                    int vectorID = *(reinterpret_cast<int*>(vectorInfo));
                    if(checked.find(vectorID) != checked.end() || m_versionMap->Deleted(vectorID)) {
                        continue;
                    }
                    checked.insert(vectorID);
                    if (VID != -1 && VID == vectorID) LOG(Helper::LogLevel::LL_Info, "Find %d in %dth posting\n", VID, i);
                    auto distance2leaf = p_index->ComputeDistance(queryResults.GetQuantizedTarget(), vectorInfo + m_metaDataSize);
                    if (distance2leaf < 1e-6) return vectorID;
                }
            }
            return -1;
        }

        void ForceGC(VectorIndex* p_index) override {
            for (int i = 0; i < p_index->GetNumSamples(); i++) {
                if (!p_index->ContainSample(i)) continue;
                Split(p_index, i, false);
                //ConcurrentSplit(p_index, i, false);
            }
        }

        void CheckCache(std::shared_ptr<VectorIndex> p_index){
            std::unordered_map<SizeType, std::string> map;

            m_postingVersionMap->check(map);

            for(auto it = map.begin(); it != map.end(); it++){
                std::string postingList;
                if (db->Get(it->first, &postingList) != ErrorCode::Success) {
                    LOG(Helper::LogLevel::LL_Info, "Get old posting value failed!\n");
                    exit(0);
                }
                m_postingVersionMap->SetNormal(it->first);

                std::unordered_set<int> vids;
                uint8_t* front_addr = reinterpret_cast<uint8_t*>(&postingList.front());
                int vec_num = postingList.size() / m_vectorInfoSize;
        
                for(int j = 0; j < vec_num; j++){
                    
                    uint8_t* vector_info = front_addr + j * m_vectorInfoSize;
                    SizeType vid = *(reinterpret_cast<SizeType*>(vector_info));

                    vids.insert(vid);
                }

                front_addr = reinterpret_cast<uint8_t*>(&(it->second).front());
                vec_num = it->second.size() / m_vectorInfoSize;

                for(int j = 0; j < vec_num; j++){
                    
                    uint8_t* vector_info = front_addr + j * m_vectorInfoSize;
                    SizeType vid = *(reinterpret_cast<SizeType*>(vector_info));

                    if(vids.count(vid) == 0){
                        vids.insert(vid);
                        std::string str(m_vectorInfoSize, '\0');
                        memcpy((char*)(str.c_str()), vector_info, m_vectorInfoSize);
                        postingList.append(str);
                    }
                    
                }

                if(db->Put(it->first, postingList) != ErrorCode::Success){
                    LOG(Helper::LogLevel::LL_Info, "Put new posting value failed!\n");
                    exit(0);
                }

                if(postingList.size() / m_vectorInfoSize > m_postingSizeLimit){
                    m_postingVersionMap->Split(it->first);
                    ConcurrentSplitAsync(p_index.get(), it->first);
                }
            }
        }

        bool AllFinished() { 
            return m_splitThreadPool->allClear() && m_reassignThreadPool->allClear(); 
        }
        void ForceCompaction() override { db->ForceCompaction(); }
        void GetDBStats() override { 
            db->GetStat();
            LOG(Helper::LogLevel::LL_Info, "remain splitJobs: %d, reassignJobs: %d, running split: %d, running reassign: %d\n", m_splitThreadPool->jobsize(), m_reassignThreadPool->jobsize(), m_splitThreadPool->runningJobs(), m_reassignThreadPool->runningJobs());
            LOG(Helper::LogLevel::LL_Info, "current posting num in postingSizes: %d, current total posting num in postingVersionMap: %d, current valid posting num in postingVersionMap: %d\n", m_postingSizes.GetPostingNum(), m_postingVersionMap->GetPostingNum(), m_postingVersionMap->GetvValidPostingNum());
        }

        void GetIndexStats(int finishedInsert, bool cost, bool reset) override { m_stat.PrintStat(finishedInsert, cost, reset); }

        bool CheckValidPosting(SizeType postingID) override {
            // if(postingID == 11){
            //     std::string postingList;
            //     db->Get(11, &postingList);
            //     std::string s = "s";
            // }
            return m_postingSizes.GetSize(postingID) > 0;
        }

        bool Initialize() override {
            return db->Initialize();
        }

        bool ExitBlockController() override {
            return db->ExitBlockController();
        }

        void GetWritePosting(SizeType pid, std::string& posting, bool write = false) override { 
            if (write) {
                db->Put(pid, posting);
                m_postingSizes.UpdateSize(pid, posting.size() / m_vectorInfoSize);
                // LOG(Helper::LogLevel::LL_Info, "PostingSize: %d\n", m_postingSizes.GetSize(pid));
                // exit(1);
            } else {
                db->Get(pid, &posting);
            }
        }

        void InitPostingRecord(std::shared_ptr<VectorIndex> p_index) {
            m_postingSizes.Initialize((SizeType)(p_index->GetNumSamples()), p_index->m_iDataBlockSize, p_index->m_iDataCapacity);
        }

        void ShowPostingDistribution(int num, bool needPrint){
            int total = 0;

            if(needPrint){
                std::string file_name = "/home/lyh/PostingDistributions/SPFresh/PostingDistribution"+std::to_string(num)+".csv";
                std::ofstream file(file_name);
                
                if (!file.is_open()) {
                    needPrint = false;
                }
                
                for(int i = 0; i < m_postingSizes.GetPostingNum(); i++){
                    //std::cout<<"("<<i<<","<<m_postingSizes.GetSize(i)<<")"<<std::endl;
                    file<<i<<","<<m_postingSizes.GetSize(i)<<"\n";
                    total += m_postingSizes.GetSize(i);
                }

                file.close();
            }
            else{
                int valid_total_num = 0;
                int valid_less_than_merge_num = 0;
                int valid_great_than_split_num = 0;
                int valid_less_than_mid_num = 0;
                int valid_great_than_mid_num = 0;

                
                for(int i = 0; i < m_postingSizes.GetPostingNum(); i++){
                    //std::cout<<"("<<i<<","<<m_postingSizes.GetSize(i)<<")"<<std::endl;
                    int size = m_postingSizes.GetSize(i);
                    total += size;

                    if(size != 0){
                        valid_total_num++;

                        if(size <= m_opt->m_mergeThreshold){
                            valid_less_than_merge_num++;
                        }
                        else if(size > m_postingSizeLimit){
                            valid_great_than_split_num++;
                        }
                        else if(size < (m_postingSizeLimit + m_opt->m_mergeThreshold)/2){
                            valid_less_than_mid_num++;
                        }
                        else{
                            valid_great_than_mid_num++;
                        }
                    }

                }

                LOG(Helper::LogLevel::LL_Info, "vaild posting num: %d, <= merge threshold: %d, > split threshold: %d, (merge threshold, mid): %d, [mid, split threshold]: %d\n", valid_total_num, valid_less_than_merge_num, valid_great_than_split_num, valid_less_than_mid_num, valid_great_than_mid_num);
            }

            LOG(Helper::LogLevel::LL_Info, "current memory vector num:%d\n", total);

        }

        void ShowBranchCounter(){
            LOG(Helper::LogLevel::LL_Info, "delete branch: %d, normal branch: %d, split or merge branch: %d\n", delete_branch, normal_branch, split_merge_branch);
            LOG(Helper::LogLevel::LL_Info, "db_get_num: %d, db_put_num: %d, search_count: %d\n", db_get_num.load(), db_put_num.load(), search_count.load());
        }

        void calculatePostingSizeMSE(){
            int posting_num = m_postingSizes.GetPostingNum();

            
            int vec_num = 0;
            int t_posting_num = posting_num;
            int small_posting_num = 0;


            int hash_value = 10;
            int interval_num = static_cast<int>(ceil(static_cast<double>(m_postingSizeLimit) / hash_value));
            std::vector<int> postingSizeCounters(interval_num + 2, 0);

            for(int i = 0; i < posting_num; i++){
                int s = m_postingSizes.GetSize(i);
                if(s == 0){
                    t_posting_num--;
                    continue;
                }
              
                if(s < (m_opt->m_mergeThreshold + m_postingSizeLimit)/2){
                    small_posting_num++;
                }

                int pos = s / hash_value;
                if(pos < 0) pos = -1;
                else if(pos > interval_num) pos = interval_num;

                postingSizeCounters[pos + 1]++;
                vec_num += s;
            }

            double average = vec_num / static_cast<double>(t_posting_num);

            double mse = 0.0;
            for(int i = 0; i < posting_num; i++){
                int s = m_postingSizes.GetSize(i);

                if(s == 0){
                    continue;
                }

                double d = static_cast<double>(s) - average;
                mse += d*d;
            }

            mse = mse / static_cast<double>(t_posting_num);

            LOG(Helper::LogLevel::LL_Info, "posting num: %d, vec num(including replicas): %d, posting average size: %.5lf, posting size mse: %.5lf, small posting ratio: %.5lf\n", t_posting_num, vec_num, average, mse, static_cast<double>(small_posting_num)/t_posting_num);

            std::string file_name = "/home/lyh/PostingDistributions/SPFresh/PostingDistribution.csv";
            std::ofstream file(file_name, std::ios::app);
            file<<t_posting_num<<",";
            for(int m = 0; m < postingSizeCounters.size(); m++){
                
                if(m == postingSizeCounters.size() - 1) file<<postingSizeCounters[m]<<"\n";
                else file<<postingSizeCounters[m]<<",";
            }

        }

    private:

        int m_metaDataSize = 0;
        
        int m_vectorInfoSize = 0;

        int m_postingSizeLimit = INT_MAX;

        std::chrono::microseconds m_hardLatencyLimit = std::chrono::microseconds(2000);

        int m_mergeThreshold = 10;
    };
} // namespace SPTAG
#endif // _SPTAG_SPANN_EXTRADYNAMICSEARCHER_H_
