import numpy as np
import torch
import math
import psutil
import os
from torch import nn
from torch.nn import functional as f
from torch.utils import data


d_dim = 2
sequence_length = 110
batch_size = 100
model_path = 'TAE.pt'
vector_embedding_dim = 256




class PositionalEncoding(nn.Module):
    def __init__(self, d_model, max_seq_len=1000):
        super().__init__()

        pe = torch.zeros(max_seq_len, d_model)
        position = torch.arange(0, max_seq_len, dtype=torch.float).unsqueeze(1)
        div_term = torch.exp(torch.arange(0, d_model, 2).float() * (-math.log(10000.0) / d_model))
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        pe = pe.unsqueeze(0).transpose(0, 1)
        self.register_buffer('pe', pe)

    def forward(self, x):
        x = x + self.pe[:x.size(0), :]
        return x


#position_encoding = PositionalEncoding(d_model=d_dim)


class TAE(nn.Module):
    def __init__(self):
        super().__init__()
        
        self.position_encoding = PositionalEncoding(d_dim)
        self.encoder_layer = nn.TransformerEncoderLayer(d_model=d_dim, nhead=2, dropout=0.1, batch_first=True)
        self.t_e = nn.TransformerEncoder(self.encoder_layer, num_layers=4)
        
        self.e_nn = nn.Linear(d_dim * sequence_length, vector_embedding_dim)

        self.decoder1 = nn.Linear(vector_embedding_dim, 64)
        self.decoder2 = nn.Linear(64, 64)

        self.flatten = nn.Flatten()      
        self.output = nn.Linear(64, d_dim * sequence_length)

    def forward(self, tv):
        
        tv_out = self.t_e(tv)

        tv_out = self.flatten(tv_out)

        tv_out = self.e_nn(tv_out)
        tv_out = f.relu(self.decoder1(tv_out))
        tv_out = f.relu(self.decoder2(tv_out))

        out = self.output(tv_out)
        out = out.reshape(batch_size, 110, 2)

        return out

device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
TAE1 = TAE().to(device)



class MyDataset(data.Dataset):
    def __init__(self, data1, labels):
        self.data1 = data1
        self.labels = labels

    def __getitem__(self, index):
        vehicle, target = self.data1[index], self.labels[index]
        return vehicle, target

    def __len__(self):
        return self.data1.size(0)


def train_model(train_iter , TAE1, device, path):
    
    loss_MSE = torch.nn.MSELoss(reduction='sum')

    epoch_num = 100
    print(epoch_num, 'start training.')
    opt = torch.optim.Adam(TAE1.parameters(), lr=0.0001)
    for epoch in range(epoch_num):
        TAE1.train()
        loss_list = []
        for x, y in train_iter:
            x, y = x.to(device), y.to(device)
            y_pre = TAE1(x)
            loss = loss_MSE(y_pre, y)
            loss_list.append(loss)
            opt.zero_grad()
            loss.backward()
            opt.step()
        print(epoch, sum(loss_list) / len(loss_list))
        TAE1.eval()

    torch.save(TAE1, path)
    return TAE1

def load_model(path):
    return torch.load(path)


if os.path.isfile(model_path):
    TAE1 = load_model(model_path)


def get_single_embedding(data_):
    global TAE1

    data_ = torch.tensor(data_, dtype=torch.float32)
    data_ = data_.reshape(1,sequence_length,d_dim)
    input = data.DataLoader(MyDataset(data_, data_), batch_size=1, shuffle=False, drop_last=False)

    for x, y in input:
        
        return TAE1.e_nn(TAE1.flatten(TAE1.t_e(x)))
        



def get_embedding(trajectory_data):
    global TAE1

    trajectory_data = torch.tensor(trajectory_data, dtype=torch.float32)
    train_iter = data.DataLoader(MyDataset(trajectory_data, trajectory_data), batch_size=batch_size,
                                shuffle=False, drop_last=True)

    

    if os.path.isfile(model_path):
        TAE1 = load_model(model_path)
    else:
        print('train model')
        TAE1 = train_model(train_iter, TAE1, device, model_path)




if __name__ == '__main__':

    
    trajectory_data = np.load("Test_Trajectory_query_data.npy")

    trajectory_data = torch.tensor(trajectory_data, dtype=torch.float32)
    train_iter = data.DataLoader(MyDataset(trajectory_data, trajectory_data), batch_size=batch_size,
                                shuffle=False, drop_last=True)

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    TAE1 = TAE().to(device)

    loss_MSE = torch.nn.MSELoss(reduction='sum')

    epoch_num = 1
    opt = torch.optim.Adam(TAE1.parameters(), lr=0.0001)
    for epoch in range(epoch_num):
        TAE1.train()
        loss_list = []
        for x, y in train_iter:
            x, y = x.to(device), y.to(device)
            y_pre = TAE1(x)
            loss = loss_MSE(y_pre, y)
            loss_list.append(loss)
            opt.zero_grad()
            loss.backward()
            opt.step()
        print(epoch, sum(loss_list) / len(loss_list))
        TAE1.eval()

    data_embedding = torch.empty(0, vector_embedding_dim).to(device)
    print(data_embedding.shape)

    num = 0

    for x, y in train_iter:
        x = x.to(device)
        
        embedding = TAE1.e_nn(TAE1.flatten(TAE1.t_e(x)))

        data_embedding = torch.cat((data_embedding, embedding), dim=0)
        

        num+=1
        mem = psutil.virtual_memory()
        print(num, mem.percent)
        
        if num == 100:
            break;
        
        #print(data_embedding.element_size()*data_embedding.nelement())
        
    print(data_embedding)
    print(data_embedding.shape)

    #torch.save(data_embedding, 'Test_Trajectory_query_tensor_data.pt')