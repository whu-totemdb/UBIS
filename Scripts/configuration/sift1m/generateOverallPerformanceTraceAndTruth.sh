setname="6c VectorPath=/home/lyh/datasets/embeddings/sift1m/batches/base_embeddings"
truthname="18c TruthPath=/home/lyh/datasets/embeddings/sift1m/batches/base_embeddings_truth"
querysize="12c QuerySize="
ride=10
start=0
end=10
dimension=128

for ((i=start; i<=end; i+=ride)); 
do
    
{  
    newsetname=$setname$i 
    newtruthname=$truthname$i 
    newquerysize=$querysize$[i+1] 
    sed -i "$newsetname" /home/lyh/SPFresh/Script_AE/configuration/sift1m/genTruth.ini  #�޸�genTruth.ini �ļ��е����е�VectorPathֵ
    sed -i "$newtruthname" /home/lyh/SPFresh/Script_AE/configuration/sift1m/genTruth.ini  #�޸�genTruth.ini �ļ��е�ʮ���е�TruthPathֵ
    sed -i "$newquerysize" /home/lyh/SPFresh/Script_AE/configuration/sift1m/genTruth.ini  #�޸�genTruth.ini �ļ��е�ʮ���е�TruthPathֵ

    /home/lyh/SPFresh/Debug/usefultool -BuildVector true --vectortype float --BaseVectorPath /home/lyh/datasets/embeddings/sift1m/base_embeddings.bin --CurrentListFileName /home/lyh/datasets/embeddings/sift1m/batches/base_embedding_ids --QueryVectorPath /home/lyh/datasets/embeddings/sift1m/query_embeddings.bin --BaseVectorSplitPath /home/lyh/datasets/embeddings/sift1m/query_vector_range.bin --filetype DEFAULT --ride $ride -d $dimension -f DEFAULT --Batch $i -NewDataSetFileName /home/lyh/datasets/embeddings/sift1m/batches/base_embeddings -NewQueryDataSetFileName /home/lyh/datasets/embeddings/sift1m/batches/query_embeddings
    
    
    
    #/home/lyh/SPFresh/Release/ssdserving /home/lyh/SPFresh/Script_AE/iniFile/genTruth.ini 
    
    
    
    #/home/lyh/SPFresh/Release/usefultool -ConvertTruth true --vectortype int8 --filetype DEFAULT --CurrentListFileName /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/batches/base_embedding_ids -d 256 --Batch $i -f DEFAULT --truthPath /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/truth/truth_embeddings_ --truthType DEFAULT --querySize $[i+1] --resultNum 10  
    
    
    #/home/lyh/SPFresh/Release/usefultool -CallRecall true -resultNum 10 -queryPath /home/lyh/datasets/embeddings/test/batches/query_embeddings$i -searchResult /home/lyh/datasets/embeddings/test/batches/base_embeddings_truth$i.dist.bin -truthType DEFAULT -truthPath /home/lyh/datasets/embeddings/test/truth/truth_embeddings_$i -VectorPath /home/lyh/datasets/embeddings/test/batches/base_embeddings$i --vectortype float -d 256 -f DEFAULT |tee /home/lyh/datasets/embeddings/test/logs/log_spfresh_$i #if merge is needed, add -CallRecall true -resultNum 10 -searchResult /home/lyh/datasets/embeddings/test/batches/base_embeddings_truth$i.dist.bin -truthType DEFAULT -truthPath /home/lyh/datasets/embeddings/test/truth/truth_embeddings_$i.    other parameters already exists.
} 

done

#rm -rf /home/lyh/datasets/embeddings/test/last_base_vector_id.txt
#get search results
#for i in {0..0..100}
#do

#{
    
#    /home/lyh/SPFresh/Release/ssdserving /home/lyh/SPFresh/Script_AE/iniFile/genTruth.ini 
    
    
    
#    /home/lyh/SPFresh/Release/usefultool -ConvertTruth true --vectortype int8 --filetype DEFAULT --CurrentListFileName /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/batches/base_embedding_ids -d 256 --Batch $i -f DEFAULT --truthPath /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/truth/truth_embeddings_ --truthType DEFAULT --querySize $[i+1] --resultNum 10  
    
    
#    /home/lyh/SPFresh/Release/usefultool -CallRecall true -resultNum 10 -queryPath /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/batches/query_embeddings$i -searchResult /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/batches/base_embeddings_truth$i.dist.bin -truthType DEFAULT -truthPath /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/truth/truth_embeddings_$i.after -VectorPath /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/batches/base_embeddings$i --vectortype int8 -d 256 -f DEFAULT |tee /home/lyh/datasets/argoverse2/motion_forecast/embeddings/test/logs/log_spfresh_$i 
    #searchResult��Ӧ���ļ�Ϊssdserving�����Ľ����truthPathΪusefultool��convertTruthѡ������Ľ��������convertTruth���truthPathΪgroundTruth�����ļ�·������������ssdserving������
    #24984
#}&

#done

#wait