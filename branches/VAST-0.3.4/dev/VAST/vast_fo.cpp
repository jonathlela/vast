/*
 * VON - forwarding model (based on VAST)
 * Copyright (C) 2006 Tsu-Han Chen (bkyo0829@yahoo.com.tw)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include "vast_fo.h"
#include "assert.h"
#include "zlib.h"
//#include "stdlib.h"
#include "vor_SF.h"
//---------------------------------------------------------------------------
namespace VAST
{
    char VAST_FO_MESSAGE[][20] = 
    {
        "DISCONNECT",
        "FO_ID",
        "FO_QUERY",
        "FO_HELLO",
        "FO_EN",
        "FO_MOVE",
        "FO_UNKNOWN",
        "FO_ZIPACK"
    };
//---------------------------------------------------------------------------
    vast_fo::vast_fo (network *netlayer, aoi_t detect_buffer, int conn_limit)
    :_connlimit(conn_limit)
    {
        this->setnet (netlayer);

         _gateway = gateway;
         _id_count = (is_gateway ? NET_ID_GATEWAY + 1 : 0);
         passing_node = false;
         fo_conn_limit = conn_limit;
         mode=1;
         _voronoi = new vor_SF ();
    }
//---------------------------------------------------------------------------
    vast_fo::~vast_fo ()
   {
       // delete allocated memory
       map<id_t, msg_Node>::iterator it = _id2node.begin ();
       for (; it != _id2node.end (); it++)
           delete _EN_lists[it->first];
   }
//---------------------------------------------------------------------------
   int vast_fo::join (id_t id, aoi_t AOI, Position &pos, Addr &gateway)
    {
       
        _original_aoi = AOI;
        _self.aoi = AOI;
        _self.pos = pt;
        loss_rate_p = 0;
        initial_count =0;
        _net->start ();

        // gateway's id is defaulted to '1'
        // all other nodes need to request for id
        if (is_gateway () == true)
        {
            _self.id = NET_ID_GATEWAY;
            _net->register_id (_self.id);
            
            insert_node (_self, _net->getaddr (_self.id));
            
            _joined = true;
            //_net->sendmsg (1, FO_ID, 0, 0, _time);
            myaddr=_net->getaddr (_self.id); 
            
            printf("gateway register");
        }
        else
        {
            // if this is a first-time join
            if (_self.id == 0)
            {
                _net->connect (1, _gateway);
                _net->sendmsg (1, FO_ID, 0, 0, _time);                 
            }
            else
            {
                _joined = true;
                
                // send out query for acceptor
                char buf[VAST_BUFSIZ];
                char *MyMsg=buf;
                msg_Node temp_self=msg_Node(_self,myaddr);
                
                memcpy(MyMsg,&temp_self,sizeof(msg_Node));
               
                _net->connect (1, _gateway);
                _net->sendmsg (1, FO_QUERY, (char *)&MyMsg, sizeof (msg_Node), _time);
            }
        }
         
        // forward only model initial
        Package_id = 0;

        //check sent node in one frame
        frame_id = 0;

        computing_count=0;
        net_latency=0;
        accept_initial=false;
        conn_count=0;
        sendmsg_bool = false;
        move_start = false;
        path_optimize=true;

        _net->flush ();
        return 0;
    }
    
    // quit VON
//---------------------------------------------------------------------------
    void vast_fo::leave ()
    {
        // remove & disconnect all connected nodes (include myself)
        vector<id_t> remove_list;
        map<id_t, msg_Node>::iterator it = _id2node.begin ();
        for (; it != _id2node.end (); it++)
            if (it->first != _self.id)
                remove_list.push_back (it->first);

        int size = remove_list.size ();
        for (int i=0; i<size; ++i)
            delete_node (remove_list[i]);
        _net->stop ();
        _joined = false;
    }

    // AOI related functions
//---------------------------------------------------------------------------
    void vast_fo::setAOI (aoi_t radius)
    {
        _self.aoi = radius;
    }
//---------------------------------------------------------------------------
    aoi_t vast_fo::getAOI ()
    {
        return _self.aoi;
    }
//---------------------------------------------------------------------------    
    // move to a new position
    Position & vast_fo::setpos (Position &pt)
    {

        // if id is not received, then do not move
        if (_self.id == 0 || (!accept_initial && _self.id!=1 && _time!=0) || pt.x==-1)//becourse node 1 is gateway needn't initial
        {  _time++;  sendmsg();  return _self.pos;}
        
        move_start=true;
        conn_count = 0;


        // check for overlap with neighbors and make correction
        if(passing_node)
        {
          for (map<id_t, msg_Node>::iterator it = _id2node.begin(); it != _id2node.end(); ++it)
            {
                if (it->first == _self.id)
                    continue;
                if (it->second.node.pos == pt)
                {
                    pt.x++;
                    it = _id2node.begin();
                }
            }    
        }
        else
        {
           //avoid node overlapping
           //pt = try_to_walk(pt);
        }
        
        _self.pos = pt; 
        sendmsg();

        // let logical time progress
        _time++;
        update_node (_self);

        return _self.pos;
    }
//---------------------------------------------------------------------------
    Position vast_fo::try_to_walk(Position pt)
    {
        float Gdifx,Gdify;
        float movex,movey;
        Position MyPos=_self.pos;
        Position Goal=pt;
        Position mid_pos;
        while(1)
        {
          Gdifx=(float)(MyPos.x-Goal.x);
          Gdify=(float)(MyPos.y-Goal.y);
          double slope;
          if(Gdify==0)
            if(Gdifx<0)slope=-99;
            else slope=99;
          else slope=Gdifx/Gdify;

          if(Gdify!=0||Gdifx!=0)
          {
            movex=0;movey=0;
            if ((Gdifx<=1&&Gdifx>=-1)&&(Gdify<=1&&Gdify>=-1))
            {
                movex =- Gdifx;
                movey =- Gdify;
            }
            else
            {
              if(Gdifx>0)
              {
                    if(Gdify>0)
                    {
                       if(slope<=0.41421356237){movex=0;movey=-1;}
                       else if(slope<=2.41421356237){movex=-1;movey=-1;}
                       else {movex=-1;movey=0;}
                    }
                    else if(Gdify==0){movex=-1;movey=0;}
                    else if(Gdify<0)
                    {
                       if(slope>=-0.41421356237){movex=0;movey=1;}
                       else if(slope>=-2.41421356237){movex=-1;movey=1;}
                       else {movex=-1;movey=0;}

                    }
              }
              else if(Gdifx<0)
              {
                    if(Gdify>0)
                    {
                       if(slope>=-0.41421356237){movex=0;movey=-1;}
                       else if(slope>=-2.41421356237){movex=1;movey=-1;}
                       else {movex=1;movey=0;}
                    }
                    else if(Gdify==0){movex=1;movey=0;}
                    else if(Gdify<0)
                    {
                       if(slope<=0.41421356237){movex=0;movey=1;}
                       else if(slope<=2.41421356237){movex=1;movey=1;}
                       else {movex=1;movey=0;}
                    }
              }
              else if(Gdifx==0)
              {
                  if(Gdify>0){movex=0;movey=-1;}
                  else if(Gdify<0){movex=0;movey=1;}
              }
            }

            if(movex!=0||movey!=0)
            {
                mid_pos.x=MyPos.x+movex;
                mid_pos.y=MyPos.y+movey;
            }

          }
          for (map<id_t, msg_Node>::iterator it = _id2node.begin(); it != _id2node.end(); ++it)
            
          {
             if (it->second.node.pos == mid_pos)
             {
                 return MyPos;
             }
          }
          if(mid_pos==Goal)return Goal;
          else MyPos=mid_pos;
        }
    }
//---------------------------------------------------------------------------
    bool vast_fo::insert_node (Node &node, Addr &addr)
    {
        if (is_neighbor (node.id))
            return false;
        
        if (node.id != _self.id)
          if(_net->connect (node.id, addr)!=0)printf("connect error\n");
//      char *ipaddrs;
        char ipa[15];
        addr.publicIP.get_string(ipa);
        //printf("%d insert node %d(%-15s,%d)\n",_self.id,node.id,ipa,addr.publicIP.port);
        _voronoi->insert (node.id, node.pos);
       
        _id2node[node.id].node = node;
        _id2node[node.id].addr = addr;
        _neighbors.push_back (&_id2node[node.id].node);
        _EN_lists[node.id] = new map<id_t, int>;


        return true;
    }
//---------------------------------------------------------------------------
    bool vast_fo::delete_node (id_t id)
    {
        // protect against deleting self or enclosing neighbors
        if (is_neighbor (id) == false || id == _self.id )
            return false;

        _net->disconnect (id);
        //printf("disconnect %d\n",id);
        _voronoi->remove (id);
        vector<Node *>::iterator it = _neighbors.begin();
        for (; it != _neighbors.end (); it++)
        {
            if ((*it)->id == id)
            {
                _neighbors.erase (it);
                break;
            }
        }

        _id2node.erase (id);
        delete _EN_lists[id];
        _EN_lists.erase (id);
        

        return true;
    }
//---------------------------------------------------------------------------
    bool vast_fo::update_node (Node &node)
    {
        if (is_neighbor (node.id) == false)
            return false;

        _voronoi->update (node.id, node.pos);
        _id2node[node.id].node.update (node);

        return true;
    }
//---------------------------------------------------------------------------

    bool vast_fo::handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size)
    {        
        new_nbr.clear();
        
        if (!accept_initial)
            initial_count++;
        if (initial_count==10)
        {
            initial_count=0;
            char buf[VAST_BUFSIZ];
            char *c=buf;
            msg_Node temp_self=msg_Node(_self,myaddr);
            memcpy(c,&temp_self,sizeof(msg_Node));
            _net->connect (1, _gateway);
            _net->sendmsg (1, FO_QUERY, c, sizeof(msg_Node), _time);
        }

        char        msg_o[CHUNK];
        int         msg_count = 0;

        bool inflating=false;
        Buffer message_vector;
        int vector_pos=0;
        vector<msg_Node> msg_FO_QUERY_vector;
        //AnsiString  AMsg;
        msg_Node Current;


#ifdef DEBUG_DETAIL
        printf ("%4d [%3d] processmsg from: %3d type: %-8s size:%3d\n", _time, (int)_self.id, from_id, VAST_FO_MESSAGE[msgtype], size);
#endif        
        if(msgtype==FO_ZIPACK)size = inf((unsigned char *)msg_o, (unsigned char *)msg, size);
        else memcpy(msg,msg_o,size);
        msg_count++;
                    
        inflating=false;
        vector_pos=0;
        
        while(1){
            if(inflating)
            {
                msgtype=message_vector.type_of_msg[vector_pos];
                size=message_vector.size_of_msg[vector_pos];
                memcpy(msg,(char *)message_vector.msg[vector_pos].c_str(),message_vector.size_of_msg[vector_pos]);
                
                if(vector_pos == (int)(message_vector.type_of_msg.size()-1))
                {
                    inflating=false;
                    message_vector.size_of_msg.clear();
                    message_vector.type_of_msg.clear();
                    message_vector.msg.clear();
                    
                }
                else vector_pos++;
            }
            
            switch ((VAST_FO_Message)msgtype)
            {
              case FO_ID:
              {
                //| id |
                if (is_gateway () == false)
                {
                    // obtain unique id from gateway
                
                    if (_self.id == 0 && size == sizeof (id_t) + sizeof (timestamp_t))
                    {
                        _self.id = *(id_t *)msg;
                        _net->register_id (_self.id);
                        myaddr=_net->getaddr(_self.id);
                        
                        insert_node (_self, myaddr);
                        timestamp_t server_time;
                        memcpy (&server_time, msg + sizeof(id_t), sizeof(timestamp_t));
                        _joined = true;
                        
                        char ip[17];
                        char *ip_ptr=ip;
                        myaddr.publicIP.get_string(ip_ptr);
                       
                        //send FO_QUERY
                        char buf[VAST_BUFSIZ];
                        char *c=buf;
                        msg_Node temp_self=msg_Node(_self,myaddr);
                        memcpy(c,&temp_self,sizeof(msg_Node));
                        _net->connect (1, _gateway);
                        _net->sendmsg (1, FO_QUERY, c, sizeof(msg_Node), _time);
                        _time = server_time;
                    }
                    else
                        ERROR_MSG_ID ( ("I'm not gateway, cannot assign id\n") );
                }
                else
                {
                    // assign a new unique id to the requester
            
                    char buf[10]; 
                    memcpy (buf, &_id_count, sizeof (id_t));
                    memcpy (buf + sizeof (id_t), &_time, sizeof (timestamp_t));
            
                    _net->sendmsg (from_id, FO_ID, (char *)buf, sizeof(id_t) + sizeof(timestamp_t), _time);
                    _id_count++;
            
                }
            
            
              }
              break;
              case FO_EN:
              {
            
                   //| msg_Node | ids |
                   msg_Node tempNod;
                   id_t tempid;
            
                   bool OldEN=false;
                   //memcpy (&Current, msg, sizeof (msg_Node));
                   Current=*(msg_Node*)msg;
                   bool isNbr=is_neighbor (Current.node.id);
                   //if msg_Node I known or Not My enclosing than break;
                   
            
            
                   if(Current.node.id!=_self.id)
                   {
                     if(!isNbr)
                     {
                       insert_node (Current.node,Current.addr);
                     }
                     //else update_node(Current.node);
                     OldEN=_voronoi->is_enclosing(_self.id,Current.node.id);
                     MakeNewRefresher(Current,-1,2);
                     if(OldEN)
                     {
                       vector<id_t> NeighborSet;
                       vector<id_t> MissNbr;
                       vector<id_t> list=_voronoi->get_en(Current.node.id);
                       for(int i=0;i<(int)((size-sizeof(msg_Node))/sizeof(id_t));i++)
                       {
                           tempid=*(id_t *)(msg+sizeof(msg_Node)+i*sizeof(id_t));
                           NeighborSet.push_back(tempid);
                       }
                       bool miss;
                       for(int k=0;k<(int)list.size();k++)
                       {
                           miss=true;
                           for(int p=0;p<(int)NeighborSet.size();p++)
                           {
                              if(NeighborSet[p]==list[k])miss=false;
                           }
            
                           if(miss)
                           {
                              MissNbr.push_back(list[k]);
                           }
                       }
                       if(MissNbr.size()!=0)
                       {
                            //send missing nbr
                            int sizem=sizeof(msg_Node)*MissNbr.size();
                            char buf[VAST_BUFSIZ];
                            char *tempBuf=buf;
            
                            int pos=0;
                            for(int k=0;k<(int)MissNbr.size();k++)
                            {
                               memcpy(tempBuf+pos,&_id2node[MissNbr[k]],sizeof(msg_Node));
                               pos+=sizeof(msg_Node);
                            }
                            MakeNewBuffer(Current.node.id);
                            SaveMessage(Current.node.id,FO_HELLO,tempBuf,sizem);
                            
            
                       }
                     }
                     else
                     {
            
                        if(!isNbr||is_new_nbr(Current.node.id))findPos(Current);
                     }
                     
                   }
              }
              break;
              
              case FO_QUERY:     //InitialForward
              {
                   //| msg_Node |
                   
                   Current=*(msg_Node *)msg;
                   findPos(Current);
              
              }
              break;
              case FO_HELLO:     //Initial
              {
                   //| msg_Nodes |
                   
                   string Check="";
                   string tempBuf="";
                   vector<id_t> NewNbr;
                   
                   for(int i=0;i<(int)(size/sizeof(msg_Node));i++)
                   {
                      Current=*(msg_Node*)(msg+i*sizeof(msg_Node));
                      
                      bool isNbr=is_neighbor (Current.node.id);
                      if(!isNbr)
                      {
                        if(Current.node.id!=_self.id)
                        {
                          accept_initial=true;
                          insert_node (Current.node,Current.addr);
                          MakeNewRefresher(Current,-1, 2);
                          MakeNewBuffer(Current.node.id);
              
                          if(!_voronoi->is_enclosing(Current.node.id,_self.id))
                          {
                            MyOldEN.push_back(Current.node.id);
                          }
                          //else delete_node(Current.node.id);
                          
                        }
                      }
                      else if(is_new_nbr(Current.node.id))
                      {
                        if(Current.node.id!=_self.id)
                        {
                          accept_initial=true;
                          MakeNewBuffer(Current.node.id);
              
                          if(!_voronoi->is_enclosing(Current.node.id,_self.id))
                          {
                            MyOldEN.push_back(Current.node.id);
                          }
                          
                        }
                      }
                      else
                      {
              
                         MakeNewBuffer(Current.node.id);
                         bool OldEN_bool=false;
                         for(int i=0;i<(int)MyOldEN.size();i++)
                         {
                            if(MyOldEN[i]==Current.node.id){OldEN_bool=true;break;}
                         }
                         if(!OldEN_bool&&!_voronoi->is_enclosing(Current.node.id,_self.id))MyOldEN.push_back(Current.node.id); 
                      }
                    }
              
              }
              break;
              case FO_MOVE:    //Receiver & Sender
              {
                  // | msg_Node | Fo_counter | Msg_id | Target_ids
              
                  string PureMsg="";
                  char Msg_id,FoCounter;
                  double Dself;
                  //int difx1,dify1;
                  
                  Current=*(msg_Node *)msg;
                  FoCounter=*(char *)(msg+sizeof(msg_Node));
                  Msg_id=*(char *)(msg+sizeof(msg_Node)+1);
                  vector<id_t> Target_ids;
                  int i;
                  for (i=0;i<(int)((size-sizeof(msg_Node)-2)/sizeof(id_t));i++)
                  {
                     id_t temp;
                     temp=*(id_t *)(msg+sizeof(msg_Node)+2+i*sizeof(id_t));
                     if(temp!=_self.id)Target_ids.push_back(temp);
                  }
              
                  if(Current.node.id==_self.id)break;
                  bool isNbr=is_neighbor (Current.node.id);
                  if(!isNbr)
                  {
                        insert_node(Current.node,Current.addr);
                        new_nbr.push_back(Current.node.id);
                  }
              
                  int skip=MakeNewRefresher(Current, Msg_id, FoCounter);
              
                  if((skip==0||skip==2)&&Target_ids.size()==0)break;
                 
                  Dself=_self.pos.dist(Current.node.pos);
                  if(skip==1)
                  {
              
                    bool oldFO_EN = false;
                    for (int i=0;i<(int)MyOldEN.size();i++)
                        if(MyOldEN[i]==Current.node.id)
                            oldFO_EN=true;
              
                    if (isNbr)
                        update_node(Current.node);
              
                    if(Dself<_self.aoi+5||oldFO_EN)
                    {
                       string NewFO_EN;//for Check;
              
                       if(_voronoi->is_enclosing(Current.node.id,_self.id))
                          if(oldFO_EN&&isNbr)
                             NewFO_EN=SaveOverlap(Current.node.id,1);
                    }
                  }
                  if(skip==1)
                  {
                    int adjust_aoi=Current.node.aoi+5;
                    if(Dself<adjust_aoi||_voronoi->is_enclosing(Current.node.id,_self.id))
                    {
                        for(int i=0;i<(int)Refresher.size();i++)
                        {
                            if(Refresher[i].node==Current.node.id)
                            {
                                 Refresher[i].To=from_id;
                                 if(Dself>_self.aoi+5&&!_voronoi->is_enclosing(Current.node.id,_self.id))Refresher[i].OutOfAOI=true;
                                 else Refresher[i].OutOfAOI=false;
                                 Refresher[i].FCount=FoCounter++;
                            }
                        }
                    } //end if statment
                  }//end if statment
                  int exist_pos=-1;
                  for(i=0;i<(int)FMsg.size();i++)
                  {
                       if(FMsg[i].id==Current.node.id)exist_pos=i;
                       break;
                  }
                  if(exist_pos==-1)
                  {
                     ForwardRecord NewFR;
                     NewFR.id=Current.node.id;
                     NewFR.from.push_back(from_id);
                     NewFR.flag_c=false;
                     NewFR.newly=true;
                     NewFR.Target=Target_ids;
                     FMsg.push_back(NewFR);
                  }
                  else if(skip==1)
                  {
                     FMsg[exist_pos].from.clear();
                     FMsg[exist_pos].from.push_back(from_id);
                     FMsg[exist_pos].newly=true;
                     //add new target
                     for(int i=0;i<(int)Target_ids.size();i++)
                     {
                        bool exist=false;
                        for(int j=0;j<(int)FMsg[exist_pos].Target.size();j++)
                        {
                           if(Target_ids[i]==FMsg[exist_pos].Target[j])
                           {
                              exist=true;break;
                           }
                        }
                        if(!exist)FMsg[exist_pos].Target.push_back(Target_ids[i]);
                     }
                  }
                  else //all the message's targets will be keeped
                  {
                     FMsg[exist_pos].from.push_back(from_id);
                     for(int i=0;i<(int)Target_ids.size();i++)
                     {
                        bool exist=false;
                        for(int j=0;j<(int)FMsg[exist_pos].Target.size();j++)
                        {
                           if(Target_ids[i]==FMsg[exist_pos].Target[j])
                           {
                              exist=true;break;
                           }
                        }
                        if(!exist)FMsg[exist_pos].Target.push_back(Target_ids[i]);
                     }
                  }
              }
              break;
              case FO_DISCONNECT:
              {
                  for(int i=0;i<(int)Refresher.size();i++)
                  {
                      if(Refresher[i].node==from_id)Refresher[i].counter=-1;
                      break;
                  }
                 
              }
              break;
              /*
              case PAYLOAD:
              {
                   // create a buffer
                   netmsg *newnode = new netmsg (from_id, msg, size, msgtype, recvtime, NULL, NULL);
              
                   // put the content into a per-neighbor queue
                   if (_id2msg.find (from_id) == _id2msg.end ())
                       _id2msg[from_id] = newnode;
                   else
                       _id2msg[from_id]->append (newnode, recvtime);
              }
              break;
              */
              case FO_ZIPACK:
              {               
                  inflating=true;
                  char *buf_pos=msg;
                  msgtype_t temp_type;
                  unsigned char temp_count;
                  size_t    temp_size;
                  string    temp_msg;
                  size_t    sum_size=0;
                  
                  while(1)
                  {
                      temp_type=*(msgtype_t *)buf_pos;
                      buf_pos+=sizeof(msgtype_t);
                      sum_size+=sizeof(msgtype_t);
                      
                      if(temp_type==FO_QUERY)
                      {
                         temp_size=sizeof(msg_Node);
                         
                      }
                      else
                      {
                         
                         temp_count=*(unsigned char *)buf_pos;
                         buf_pos+=sizeof(unsigned char);
                         sum_size+=sizeof(unsigned char);
                         if(temp_type==FO_EN)
                             temp_size=sizeof(msg_Node)+temp_count*sizeof(id_t);
                         else if (temp_type==FO_HELLO)
                             temp_size=sizeof(msg_Node)*temp_count;
                         else if (temp_type==FO_MOVE)
                             temp_size=sizeof(msg_Node)+2+temp_count*sizeof(id_t);
                      }
                       
                      temp_msg.assign(buf_pos,temp_size);
                      buf_pos+=temp_size;
                      sum_size=sum_size+temp_size;
                      message_vector.type_of_msg.push_back(temp_type);
                      message_vector.size_of_msg.push_back(temp_size);
                      message_vector.msg.push_back(temp_msg);
                  
                      if(sum_size== (size_t)size)
                                              break;
                  }
                 
              
              }
              break;
              default:
                  break;
            }//end switch
                  
            if(!inflating)         
                break;
         
        } //end while (inflating)

        //Refresher countdown with counter
        for(int j=0;j<(int)Refresher.size();j++)
        {
            if ((id_t)Refresher[j].To==from_id)
                        Refresher[j].counter--;
        }
                
                        
        SayHelloToNewNbr();
        while (conn_count<=fo_conn_limit || computing_count == 0)
        {
            bool sent_all=true;

            id_t action_id=0;
            int action_id_pos=-1;
            vector<id_t> list=_voronoi->get_en(_self.id);
            int i;
            for(i=0;i<(int)BufferFlow.size();i++)
            {
              conn_count++;
              //char message_buffer[CHUNK];
              //size_t message_size=0;
              //char *buf_pos=message_buffer;
              if(BufferFlow[i].frame_id != frame_id)
              {
                  sent_all=false;
                  BufferFlow[i].frame_id = frame_id;
                  action_id = BufferFlow[i].id;
                  action_id_pos = i;
                
                  break;
               }
            }
            if(action_id_pos==-1){break;}
            if(sent_all){break;sendmsg_bool=false;}
            vector<id_t> to_FO_EN_s;

            for(i=0;i<(int)Refresher.size();i++)  //find message to forward action_id
            {
               if(Refresher[i].To==action_id)to_FO_EN_s.push_back(Refresher[i].node);
            }
            
            for(i=0;i<(int)FMsg.size();i++)
            {

                char Fid=-1;
                char FCount=-1;
                for(int j=0;j<(int)Refresher.size();j++)
                {
                   if(Refresher[j].node==FMsg[i].id)
                   {       
                      FCount=Refresher[j].FCount;
                      Fid=Refresher[j].serial;
                      break;
                   }
                }


                vector<id_t> to_forward;
                for(int k=FMsg[i].Target.size()-1;k>=0;k--)
                {
                    for(int j=0;j<(int)to_FO_EN_s.size();j++)
                    {
                        if(FMsg[i].Target[k]==to_FO_EN_s[j]){to_forward.push_back(k);break;}

                    }
                }
                char buf[VAST_BUFSIZ];
                char *Msg=buf;
                memcpy(Msg,&_id2node[FMsg[i].id],sizeof(msg_Node));
                memcpy(Msg+sizeof(msg_Node),&FCount,1);
                memcpy(Msg+sizeof(msg_Node)+1,&Fid,1);

                if(to_forward.size()!=0)
                {
                    for(int k=0;k<(int)to_forward.size();k++)
                    {
                       id_t tempid=FMsg[i].Target[to_forward[k]];
                       memcpy(Msg+sizeof(msg_Node)+2+sizeof(id_t)*k,&tempid,sizeof(id_t));
                    }

                    int sizem=sizeof(msg_Node)+2+sizeof(id_t)*to_forward.size();
                    SaveMessage(action_id,FO_MOVE,Msg,sizem);


                    for(int m=0;m<(int)to_forward.size();m++)
                    {
                        FMsg[i].Target.erase(FMsg[i].Target.begin()+to_forward[m]);
                    }
                    if(FMsg[i].Target.size()!=0)FMsg[i].flag_c=true;


                }
                else if(FMsg[i].newly)
                {

                    //int difx,dify;
                    
                    bool forwarding_msg=true;
                    double temp_dist=_id2node[_self.id].node.pos.dist(_id2node[FMsg[i].id].node.pos);
                    
                    int buf_pos=get_Buffer_pos(action_id);
                    if(temp_dist<_id2node[FMsg[i].id].node.aoi+5)
                    {
                      if(path_optimize)
                      {
                         for(int k=0;k<(int)BufferFlow[buf_pos].OldEN.size();k++)
                         {                   
                                bool is_candidate=false;
                                for(int r=0;r<(int)FMsg[i].from.size();r++)
                                {
                                   if(_voronoi->is_enclosing(FMsg[i].from[r],BufferFlow[buf_pos].OldEN[k])){is_candidate=true;break;}
                                }
                                if(is_candidate)
                                {
                                  temp_dist=_id2node[BufferFlow[buf_pos].OldEN[k]].node.pos.dist(_id2node[FMsg[i].id].node.pos);
                                  
                                  
                                  unsigned long id_d_1,id_d_2;
                                  id_d_1=abs((long)(_id2node[BufferFlow[buf_pos].OldEN[k]].node.id-action_id));
                                  id_d_2=abs((long)(_self.id-action_id));
                                  if(temp_dist<_id2node[FMsg[i].id].node.aoi&&(_id2node[BufferFlow[buf_pos].OldEN[k]].node.aoi>_self.aoi
                                      ||(id_d_1<id_d_2&&_id2node[BufferFlow[buf_pos].OldEN[k]].node.aoi==_self.aoi)))
                                  {
                                     forwarding_msg=false;break;  //get another node to forwarding
                                  }
                                }
                         }
                      }
                      if(forwarding_msg)
                      {
                        temp_dist=_id2node[action_id].node.pos.dist(_id2node[FMsg[i].id].node.pos);
                        
                        if(temp_dist<_id2node[FMsg[i].id].node.aoi)
                        {
                           bool forward_bool=true;
                           for(int k=0;k<(int)FMsg[i].from.size();k++)
                           {
                              if(_voronoi->is_enclosing(FMsg[i].from[k],action_id)||action_id==FMsg[i].from[k]){forward_bool=false;break;}
                           }
                           int sizem=sizeof(msg_Node)+2;
                           if(forward_bool)
                           {
                              SaveMessage(action_id,FO_MOVE,Msg,sizem);                              
                           }

                        }
                      }
                   }

                }

            }

         if (conn_count>fo_conn_limit && computing_count!=0)
             break;

      }//end while

      int i;
      for(i=0;i<(int)FMsg.size();i++)
      {
         FMsg[i].newly=false;
      }

      if(computing_count == 0 )
      {
        
        AdjustPath();
        vector<int> old_keep_vec;

        int i;
        for(i=Refresher.size()-1;i>=0;i--)
        {
            Refresher[i].renew=true;
            if(Refresher[i].counter<0)
            {
                bool BFcheck=false;

                int j;
                for(j=0;j<(int)BufferFlow.size();j++)
                {
                    if(Refresher[i].node==BufferFlow[j].id)
                    {
                       BFcheck=true;break;
                    }
                }
                if(!BFcheck)old_keep_vec.push_back(i);

                delete_node(Refresher[i].node);
                
                //bool BFcheck to check delete node which has bufferflow


                int oldFR=-1;
                for(j=0;j<(int)FMsg.size();j++)
                {
                    if(FMsg[j].id==Refresher[i].node)
                    {
                        oldFR=j;
                        break;
                    }
                }
                if(oldFR!=-1)FMsg.erase(FMsg.begin()+oldFR);
            }
        }

        for(int m=0;m<(int)old_keep_vec.size();m++)
        {
           Refresher.erase(Refresher.begin()+old_keep_vec[m]);
        }

      }

      frame_id=(frame_id+1)%100;

      

      if(!move_start)sendmsg();
      vector<id_t> old_en;
       
        for(i=BufferFlow.size()-1;i>=0;i--)
        {
            
            bool OldENbool=false;
            for(int j=0;j<(int)MyOldEN.size();j++)
            {
               if(MyOldEN[j]==BufferFlow[i].id){OldENbool=true;break;}
            }
            if(OldENbool||_voronoi->is_enclosing(BufferFlow[i].id,_self.id))
            {
                 BufferFlow[i].lifetime=1;
            }
            else
            {
               old_en.push_back(i);
            }

        }
        int m;
        for(m=0; m<(int)old_en.size(); m++)
        {
           BufferFlow.erase(BufferFlow.begin()+old_en[m]);
        }
         //delete some useless FR
         vector<int> del_FR;
         for(i=FMsg.size()-1;i>=0;i--)
         {
            //int difx,dify;
            double temp_dist=_self.pos.dist(_id2node[FMsg[i].id].node.pos);
        
            bool Still_Forward=false;
            if(temp_dist<_id2node[FMsg[i].id].node.aoi)Still_Forward=true;
            if(FMsg[i].flag_c==false||(FMsg[i].Target.size()==0 && !Still_Forward))del_FR.push_back(i);
            FMsg[i].flag_c=false;
         }

         for(m=0;m<(int)del_FR.size();m++)
         {
            FMsg.erase(FMsg.begin()+del_FR[m]);
         }



        Package_id=((Package_id)+1)%100;
        vector<id_t> FO_ENlist=_voronoi->get_en(_self.id);
        MyOldEN=FO_ENlist;

        _net->flush ();

        return true;

    } // handlemsg ()


    // do things after messages are all handled
    // TODO: move neighbor discovery here
    void vast_fo::post_processmsg ()
    {
    }


//---------------------------------------------------------------------------
    void vast_fo::sendmsg(void)
    {
        //send new position to FO_EN  
        
        char buf[VAST_BUFSIZ];
        char *MyMsg=buf;
        char FC=1;
        msg_Node temp_self=msg_Node(_self,myaddr);
        memcpy(MyMsg,&temp_self,sizeof(msg_Node));
        memcpy(MyMsg+sizeof(msg_Node),&FC,1);
        memcpy(MyMsg+sizeof(msg_Node)+1,&Package_id,1);
        vector<Keep> temp_Refresher=Refresher;
        int i;
        for(i=BufferFlow.size()-1;i>=0;i--)
        {
            vector<id_t> out_of_aoi;
            int j;
            for(j=0;j<(int)temp_Refresher.size();j++)
            {
              if(BufferFlow[i].id== (id_t)temp_Refresher[j].To&&temp_Refresher[j].OutOfAOI)
              {
                  out_of_aoi.push_back(temp_Refresher[j].node);
                  temp_Refresher.erase(temp_Refresher.begin()+j);
                  break;
              }
            }
            
            int sizem=sizeof(msg_Node)+2+sizeof(id_t)*out_of_aoi.size();
            for(j=0;j<(int)out_of_aoi.size();j++)
            {
               
               memcpy(MyMsg+sizeof(msg_Node)+2+sizeof(id_t)*j,&out_of_aoi[j],sizeof(id_t));
            }

            //int check_send=0;
            
            
            SaveMessage(BufferFlow[i].id,FO_MOVE,MyMsg,sizem);
            conn_count++;
              char message_buffer[CHUNK];
              size_t message_size=0;
              char *buf_pos=message_buffer;
              if(BufferFlow[i].frame_id != frame_id)
              {
                                      
                    for(int j=0;j<(int)BufferFlow[i].type_of_msg.size();j++)
                    {
                       
                       memcpy(buf_pos,&BufferFlow[i].type_of_msg[j],sizeof(msgtype_t));
                       buf_pos+=sizeof(msgtype_t);
                       message_size+=sizeof(msgtype_t);
                       
                       unsigned char temp_count;
                       if(BufferFlow[i].type_of_msg[j]==FO_EN)
                       {
                           temp_count=(BufferFlow[i].size_of_msg[j]-sizeof(msg_Node))/sizeof(id_t);
                       }
                       else if(BufferFlow[i].type_of_msg[j]==FO_QUERY)
                       {
                           temp_count=0;
                       }
                       else if(BufferFlow[i].type_of_msg[j]==FO_HELLO)
                       {
                           temp_count=BufferFlow[i].size_of_msg[j]/sizeof(msg_Node);
                       }
                       else if(BufferFlow[i].type_of_msg[j]==FO_MOVE)
                       {
                           temp_count=(BufferFlow[i].size_of_msg[j]-sizeof(msg_Node)-2)/sizeof(id_t);
                       }
                       if(BufferFlow[i].type_of_msg[j]!=FO_QUERY)
                       {
                           memcpy(buf_pos,&temp_count,sizeof(unsigned char));
                           buf_pos+=sizeof(unsigned char);
                           message_size+=sizeof(unsigned char);
                       }
                       
                       memcpy(buf_pos,BufferFlow[i].msg[j].c_str(),BufferFlow[i].size_of_msg[j]);
                       buf_pos+=BufferFlow[i].size_of_msg[j];
                       message_size+=BufferFlow[i].size_of_msg[j];
                    }
                    
                
                    if(message_size!=0)net_sendmsg (BufferFlow[i].id,FO_ZIPACK,message_buffer,message_size,_time+net_latency);
                    BufferFlow[i].type_of_msg.clear();
                    BufferFlow[i].msg.clear();
                    BufferFlow[i].size_of_msg.clear();
                 
               }
         }
        
    }
//---------------------------------------------------------------------------
    void vast_fo::SaveMessage(id_t id,int type,char* msg,int size)
    {
       for(int i=0;i<(int)BufferFlow.size();i++)
       {
           if(BufferFlow[i].id==id)
           {
              BufferFlow[i].size_of_msg.push_back(size);
              BufferFlow[i].type_of_msg.push_back(type);

              string temp_str;
              temp_str.assign(msg,size);
              
              BufferFlow[i].msg.push_back(temp_str);
              return;
           }
       }

    }
//---------------------------------------------------------------------------
/*
    int  vast_fo::GetPath(int index)
    {
        for(int i=0;i<(int)Refresher.size();i++)
        {
           if(Refresher[i].node==index)
               return Refresher[i].To;
        }
        return -1;
    }
*/
//---------------------------------------------------------------------------
    void vast_fo::AdjustPath(void)
    {
        vector<id_t> list=_voronoi->get_en(_self.id);

        double Temp;
        //int Difx,Dify;
        for(int i=0;i<(int)Refresher.size();i++)
        {
           int closer=-1;
           double TempDis=-1;
           if(!_voronoi->is_enclosing(Refresher[i].To,_self.id))
           {

               bool change=false;
               for(int j=0;j<(int)FMsg.size();j++)
               {
                  if(FMsg[j].id==Refresher[i].node)
                  {
                      for(int k=0;k<(int)FMsg[j].from.size();k++)
                      {
                         if(FMsg[j].from[k]!= (id_t)Refresher[i].To && _voronoi->is_enclosing(FMsg[j].from[k],_self.id))
                         { Refresher[i].To= FMsg[j].from[k]; change=true;break;}
                      }
                      break;
                  }
               }
               if(!change)
               {
                 for(int j=0;j<(int)list.size();j++)
                 {
                   if(closer==-1)
                   {
                      closer=list[j];
                      
                     
                      TempDis=_id2node[list[j]].node.pos.dist(_id2node[Refresher[i].node].node.pos);
                   }
                   else
                   {
                      
                      Temp=_id2node[list[j]].node.pos.dist(_id2node[Refresher[i].node].node.pos);
                      if(TempDis>Temp)
                      {
                         closer=list[j];
                         TempDis=Temp;
                      }
                   }
                 }
                 Refresher[i].To=closer;
               }
           }
           
        }
    }

//---------------------------------------------------------------------------
    void vast_fo::findPos(msg_Node New)
    {
       id_t id=New.node.id;
       Position Pos=New.node.pos;
       //int AOI=New.node.aoi;
       
       //int difx1,difx2,dify1,dify2;
       bool isNbr=is_neighbor (id);
       vector<id_t> list=_voronoi->get_en(_self.id);
       if(!isNbr)
       {
          insert_node(New.node,New.addr);
          MakeNewRefresher(New,-1,2);
       }

       if(_voronoi->is_enclosing(_self.id,id))
       {

         vector<id_t> IniNbr;
         int j;
         for (j=0; j<(int)list.size(); j++)
         {
             if(_voronoi->is_enclosing(list[j],id))
             {
               IniNbr.push_back(list[j]);
             }
         }
         int size=sizeof(msg_Node)*(IniNbr.size()+1);
         char buf[VAST_BUFSIZ];
         char *sendhello=buf;
         msg_Node temp_self=msg_Node(_self,myaddr);
         memcpy(sendhello,&temp_self,sizeof(msg_Node));
         for(j=0;j<(int)IniNbr.size();j++)
            memcpy(sendhello+sizeof(msg_Node)*(j+1),&_id2node[IniNbr[j]],sizeof(msg_Node));
         MakeNewBuffer(id);
         SaveMessage(id,FO_HELLO,sendhello,size);
         
       }
       else
       {
         delete_node(id);
         int temp_id=-1;
         Position temp_pos;
         for (int j=0; j<(int)list.size(); j++)
         {
              if(temp_id==-1)temp_id=list[j];
              else
              {
                
                if(Pos.dist(_id2node[temp_id].node.pos)>Pos.dist(_id2node[list[j]].node.pos))
                {
                  temp_id=list[j];
                }
              }
         }
         int size=sizeof(msg_Node);
         char buf[VAST_BUFSIZ];
         char *to_query=buf;
         memcpy(to_query,&New,sizeof(msg_Node));
         MakeNewBuffer(temp_id);
         SaveMessage(temp_id,FO_QUERY,to_query,size);
    
       }

    }
//---------------------------------------------------------------------------
    string vast_fo::SaveOverlap(int id,int type)
    {
       int BFNum=-1;
       int Nbr= id;
       int i;
       for(i=0;i<(int)BufferFlow.size();i++)
       {
          if(BufferFlow[i].id==(id_t)id)
          {
              BFNum=i;
              break;
          }
       }
       if(BFNum==-1)return "";
       vector<id_t> list=_voronoi->get_en(_self.id);
       vector<id_t> oldOverlap;
       vector<id_t> NewOverlap;
       bool overlap;

       
       for(i=0;i<(int)list.size();i++)
       {
           if(_voronoi->is_enclosing(list[i],Nbr))NewOverlap.push_back(list[i]);
       }

       vector<id_t> NewEn;
       for(i=0;i<(int)NewOverlap.size();i++)
       {

           overlap=false;
           for(int j=0;j<(int)BufferFlow[BFNum].OldEN.size();j++)
           {
               if(NewOverlap[i]==BufferFlow[BFNum].OldEN[j])overlap=true;
           }
           if(!overlap)NewEn.push_back(NewOverlap[i]);
       }
       int size=NewEn.size()*sizeof(msg_Node);
       char buf[VAST_BUFSIZ];
       char *sendhello=buf;
       for(i=0;i<(int)NewEn.size();i++)
           memcpy(sendhello+sizeof(msg_Node)*i,&_id2node[NewEn[i]],sizeof(msg_Node));
       if(NewEn.size()!=0&&type!=0)
       {
         MakeNewBuffer(Nbr);
         SaveMessage(Nbr,FO_HELLO,sendhello,size);
         
       }
       BufferFlow[BFNum].OldEN=NewOverlap;

       return "";
    }
//---------------------------------------------------------------------------
    void vast_fo::SayHelloToNewNbr(void)
    {

       
       vector<id_t> FO_ENlist=_voronoi->get_en(_self.id);

       bool New;

       int size=sizeof(msg_Node)+sizeof(id_t)*FO_ENlist.size();
       char buf[VAST_BUFSIZ];
       char *senden=buf;
       msg_Node temp_self=msg_Node(_self,myaddr);

       memcpy(senden,&temp_self,sizeof(msg_Node));
       int i;
       for(i=0;i<(int)FO_ENlist.size();i++){

           memcpy(senden+sizeof(msg_Node)+sizeof(id_t)*i,&FO_ENlist[i],sizeof(id_t));
       }

       for(i=0;i<(int)FO_ENlist.size();i++)
       {
            New=true;

            for(int j=0;j<(int)MyOldEN.size();j++)
            {
                if(FO_ENlist[i]==MyOldEN[j])New=false;
            }
            if(New)
            {
                MakeNewBuffer(FO_ENlist[i]);
                SaveOverlap(FO_ENlist[i],1);
                SaveMessage(FO_ENlist[i],FO_EN,senden,size);
            }
            else
            {
                SaveOverlap(FO_ENlist[i],1);
            }
       }
       for(int j=0;j<(int)MyOldEN.size();j++)
       {
           bool save_new_overlapping=true;
           for(int i=0;i<(int)FO_ENlist.size();i++)
           {
              if(MyOldEN[j]==FO_ENlist[i])save_new_overlapping=false;
           }
           SaveOverlap(MyOldEN[j],1);
       }

    }
//---------------------------------------------------------------------------
    void vast_fo::MakeNewBuffer(id_t id)
    {

        for(int i=0;i<(int)BufferFlow.size();i++)
        {
           if(BufferFlow[i].id==id)
           {
             BufferFlow[i].lifetime=1;
             return;
           }
        }
        Buffer NewBuf;
        NewBuf.id=id;
        NewBuf.frame_id=-1;
        NewBuf.msg.clear();
        NewBuf.lifetime=1;
        BufferFlow.push_back(NewBuf);

    }
//---------------------------------------------------------------------------
int vast_fo::MakeNewRefresher(msg_Node Current,int serial,int life_time)
{
    int A=-1;
    int B;
    bool skip=false;
    bool samesource=false;
    bool MakeNew=true;
    for(int i=0;i<(int)Refresher.size();i++)
    {
        if(Refresher[i].node==Current.node.id)
        {
           A=Refresher[i].serial;
           B=serial;
           B=A-B;
           if(is_neighbor(Current.node.id)&&A!=-1&&((B>0&&B<90)||(B>=-99&&B<-90))&&serial!=-1||(serial==-1&&A!=-1)){skip=true;}
           else
           {
                if(B==0)samesource=true;
                
                if(!samesource)
                {
                  Refresher[i].renew=true;
                  if(serial!=-1)Refresher[i].serial=serial;
                }
                
                if(life_time==-1)Refresher[i].counter=(net_latency+1)*4; //keep AN 4 steps
                else Refresher[i].counter=(net_latency+1)*2; //keep FO_EN 2 steps
           }
           MakeNew=false;
           break;
       }
    }
    if(MakeNew)
    {

        Keep NewRef;
        NewRef.node=Current.node.id;
        NewRef.renew=true;
        NewRef.To=-1;
        NewRef.FCount=0;
        NewRef.OutOfAOI=false;
        if(life_time==-1)NewRef.counter=(net_latency+1)*4;
        else NewRef.counter=(net_latency+1)*2;
        NewRef.serial=serial;
        Refresher.push_back(NewRef);
        
    }
    if(skip)return 0;
    else if(samesource)return 2;
    else return 1;
}
//---------------------------------------------------------------------------
int vast_fo::return_CN(void)
{
    return BufferFlow.size();
}
//---------------------------------------------------------------------------
void vast_fo::op_switch(bool op_bool)
{
    path_optimize=op_bool;
}
//---------------------------------------------------------------------------
bool vast_fo::is_neighbor(id_t id)
{
    for(int i=0;i<(int)_neighbors.size();i++)
    {
       if(_neighbors[i]->id==id)
           return true;
    }
    return false;
}
//---------------------------------------------------------------------------
bool vast_fo::is_new_nbr (id_t id)
{
    for(int i=0;i<(int)new_nbr.size();i++)
    {
       if(new_nbr[i]==id)return true;
    }
    return false;
}
//---------------------------------------------------------------------------
void  vast_fo::loss_rate_setting(int lr)
{
    loss_rate_p=lr;
}
//---------------------------------------------------------------------------
char *vast_fo::getstat (bool clear)
{
    return NULL;
}
/*
//---------------------------------------------------------------------------
int vast_fo::send (id_t target, const char* data, int size, bool is_reliable, bool buffered)
{
     return _net->sendmsg (target, PAYLOAD, data, size, _time, is_reliable, buffered);
}
//---------------------------------------------------------------------------
int vast_fo::recv (id_t target, char *buffer)
{
     if (_id2msg.find (target) == _id2msg.end ())
     return 0;
        
     netmsg *queue = _id2msg[target];
     netmsg *msg;
     queue = queue->getnext (&msg);

     // update the mapping to queue
     if (queue == NULL)
         _id2msg.erase (target);
     else
         _id2msg[target] = queue;
        
        // copy the msg into buffer
     int size = msg->size;
     memcpy (buffer, msg->msg, size);
     delete msg;

     return size;
}

// send out all pending reliable messages in a single packet
int vast_fo::flush ()
{
    return _net->flush ();
}
*/

//---------------------------------------------------------------------------
int vast_fo::def(unsigned char *source,unsigned char *dest,int size)
{
    
    int ret;
    unsigned have;
    z_stream strm;
    

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, -1);
    if (ret != Z_OK)
        return ret;
    /* compress until end of file */
    strm.avail_in =size;
    strm.next_in = source;
    strm.avail_out = CHUNK;
    strm.next_out = dest;
      
    ret = deflate(&strm, Z_FINISH);    /* no bad return value */
    assert(ret != Z_STREAM_ERROR);  /* state not clobbered */

    have = CHUNK - strm.avail_out;
    (void)deflateEnd(&strm);
    return have;
          
}
//---------------------------------------------------------------------------
int vast_fo::inf(unsigned char *source,unsigned char *dest, int size)
{
    
    if(size==0)
    {
        source=dest;
        return size;
    }

    int ret;
    unsigned have;
    z_stream strm;

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = size;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
    {return ret;}
    
    strm.next_in = source;
    strm.avail_out = CHUNK;
    strm.next_out = dest;
    ret = inflate(&strm, Z_NO_FLUSH);   /* no bad return value */
    assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
    switch (ret) {
         case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
         case Z_DATA_ERROR:
         case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
    return ret;
    }
    have = CHUNK - strm.avail_out;
    (void)inflateEnd(&strm);
    
    return have;

}
//---------------------------------------------------------------------------
int vast_fo::net_sendmsg(id_t to, msgtype_t type,char *msg,size_t size,timestamp_t a_time)
{
    int error;
    if(size==0)error=_net->sendmsg (to,type,msg,size,0);
    else
    {
        int zip_size;
        char gzip[CHUNK];
        zip_size=def((unsigned char *)msg,(unsigned char *)gzip,size);
        error=_net->sendmsg (to,type,gzip,zip_size,a_time);
        
    }
    return error;
    
}
//---------------------------------------------------------------------------
int vast_fo::get_Buffer_pos(id_t id)
{
    for(int i=0;i<(int)BufferFlow.size();i++)
    {
        if(BufferFlow[i].id==id)return i;
    }
    return -1;
}
//---------------------------------------------------------------------------
void vast_fo::zip_test(void)
{
    char in1[CHUNK];
    char in2[CHUNK];
    char out[CHUNK];
    srand (time (NULL));
    msg_Node test;
    msgtype_t type;
    unsigned char fc,serial,count;
    char* pos1=in1;
    char* pos2=in2;
    int zip_size1,zip_size2;
    int length1=0,length2=0;
    unsigned int temp;

    for(int i=1;i<2000;i++)
    {
        
            type=rand()%8;
            count=rand()%256;
            fc=rand()%100;
            serial=rand()%100;
            test.node.id=0;
            test.node.pos.x=0;
            test.node.pos.y=0;
            test.node.aoi=rand()%200;
            test.addr.set_public(rand()%655350000,rand()%65535);
            
            temp=type;
            memcpy(pos1,&temp,sizeof(int));
            pos1+=sizeof(int);
            length1+=sizeof(int);
            temp=count;
            memcpy(pos1,&temp,sizeof(int));
            pos1+=sizeof(int);
            length1+=sizeof(int);
            memcpy(pos1,&test.node.id,sizeof(id_t));
            pos1+=sizeof(id_t);
            length1+=sizeof(id_t);
            memcpy(pos1,&test.node.pos,sizeof(Position));
            pos1+=sizeof(Position);
            length1+=sizeof(Position);
            temp=test.node.aoi;
            memcpy(pos1,&temp,sizeof(int));
            pos1+=sizeof(int);
            length1+=sizeof(int);
            memcpy(pos1,&test.addr.publicIP.host,sizeof(unsigned long));
            pos1+=sizeof(unsigned long);
            length1+=sizeof(long);
            temp=test.addr.publicIP.port;
            memcpy(pos1,&temp,sizeof(int));
            pos1+=sizeof(int);
            length1+=sizeof(int);
            temp=fc;
            memcpy(pos1,&temp,sizeof(int));
            pos1+=sizeof(int);
            length1+=sizeof(int);
            temp=serial;
            memcpy(pos1,&temp,sizeof(int));
            pos1+=sizeof(int);
            length1+=sizeof(int);
        
            memcpy(pos2,&type,sizeof(msgtype_t));
            pos2+=sizeof(msgtype_t);
            length2+=sizeof(msgtype_t);
            memcpy(pos2,&count,sizeof(char));
            pos2+=sizeof(char);
            length2+=sizeof(char);
            memcpy(pos2,&test,sizeof(msg_Node));
            pos2+=sizeof(msg_Node);
            length2+=sizeof(msg_Node);
            memcpy(pos2,&fc,sizeof(char));
            pos2+=sizeof(char);
            length2+=sizeof(char);
            memcpy(pos2,&serial,sizeof(char));
            pos2+=sizeof(char);
            length2+=sizeof(char);
            
        
        zip_size1=def((unsigned char *)in1,(unsigned char *)out,length1);
        zip_size2=def((unsigned char *)in2,(unsigned char *)out,length2);
        
    }
}
}


