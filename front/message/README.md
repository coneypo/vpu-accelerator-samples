# Pipeline Message Definitions and Pipeline States

## 1 Pipeline States

The mediapipe, at any given time, will be at one of these states:
- NONEXIST
- CREATED
- PLAYING
- PAUSED
- STOPPED

## 2 Pipeline Message Definitions

The HDDL manager will create an unix socket, and all created HDDL mediapipes will connect to it. The following are the message definitions between the HDDL manager and mediapipes.

The messages are categoried as:
- The message sent from HDDL manager to HDDL mediapipe, in the format of `MsgRequest`;
- The message sent from HDDL mediapipe to HDDL manager, in the format of `MsgResponse`;
    - The response messages, which answers one request sent from HDDL manager;
    - The one-shot event messages sent from mediapipe to manager, no response of manager needed;

The messages are defined as protobuf messages.

All messages will carry pipeline id.

At initialization stage, two steps of create/play requests are needed to start a pipeline.

At exiting stage, two steps stop/destroy are needed. After stop, only destroy is allowed for now, this is for possible extension in the future.

### 2.1 Request messages (manager -> mediapipe)

The request format is:
```
message MsgRequest {
    optional MsgBase           base = 1;
    optional MsgReqType        req_type = 2;
    optional uint64            req_seq_no = 3;
    optional int32             pipeline_id = 4;

    oneof msg {
        CreatePipelineRequest  create = 10;
        DestroyPipelineRequest destroy = 11;
        ModifyPipelineRequest  modify = 12;
        PlayPipelineRequest    play = 13;
        PausePipelineRequest   pause = 14;
        StopPipelineRequest    stop = 15;
    }
}

enum MsgReqType {
    CREATE_REQUEST = 0;
    DESTROY_REQUEST = 1;
    MODIFY_REQUEST = 2;
    PLAY_REQUEST = 3;
    PAUSE_REQUEST = 4;
    STOP_REQUEST = 5;
}
```

Each request message is of one of the `MsgReqType`. The `req_seq_no` is an unique number representing this request, in response message of this request the same seq. NO. should be used. The content of each request type is defined in its own message and embedded.

#### 2.1.1 CreatePipelineRequest

|||
|---|---|
| Description | After connection is established by MsgRegister, manager will send this message to mediapipe. |
| Allowed mediapipe state | NONEXIST |
| Next mediapipe state | CREATED |

#### 2.1.2 DestroyPipelineRequest

|||
|---|---|
| Description | Manager sends this message to mediapipe to destroy pipeline. |
| Allowed mediapipe state | CREATED, PLAYING, PAUSED, STOPPED |
| Next mediapipe state | NONEXIST |

#### 2.1.3 ModifyPipelineRequest

|||
|---|---|
| Description | Manager sends this message to mediapipe to destroy pipeline. |
| Allowed mediapipe state | CREATED, PLAYING, PAUSED |
| Next mediapipe state | not changed |

#### 2.1.4 PlayPipelineRequest

|||
|---|---|
| Description | Manager sends this message to mediapipe to start pipeline, or restart a pausing pipeline. |
| Allowed mediapipe state | CREATED, PAUSED |
| Next mediapipe state | PLAYING |

#### 2.1.5 PausePipelineRequest

|||
|---|---|
| Description | Manager sends this message to mediapipe to pause pipeline. |
| Allowed mediapipe state | PLAYING |
| Next mediapipe state | PAUSED |

#### 2.1.6 StopPipelineRequest

|||
|---|---|
| Description | Manager sends this message to mediapipe to stop pipeline. |
| Allowed mediapipe state | PLAYING, PAUSED |
| Next mediapipe state | STOPPED |

### 2.2 Response/Event messages (mediapipe -> manager)

The response/event format is:
```
message MsgResponse {
    optional MsgBase            base = 1;
    optional MsgRspType         rsp_type = 2;
    optional uint64             req_seq_no = 3;
    optional int32              ret_code = 4;
    optional int32              pipeline_id = 5;

    oneof msg {
        CreatePipelineResponse  create = 10;
        DestroyPipelineResponse destroy = 11;
        ModifyPipelineResponse  modify = 12;
        PlayPipelineResponse    play = 13;
        PausePipelineResponse   pause = 14;
        StopPipelineResponse    stop = 15;

        RegisterEvent           register = 20;
        MetadataEvent           metadata = 21;
    }
}

enum MsgRspType {
    CREATE_RESPONSE = 0;
    DESTROY_RESPONSE = 1;
    MODIFY_RESPONSE = 2;
    PLAY_RESPONSE = 3;
    PAUSE_RESPONSE = 4;
    STOP_RESPONSE = 5;

    REGISTER_EVENT = 10;
    METADATA_EVENT = 11;
}
```

Each response/event message is of one of the `MsgRspType`. The content of each response/event type is defined in its own message and embedded.

The response messages should carry the same `req_seq_no` as the corresponding request, and set `ret_code` to report the request executing result.

The event messages doesn't have corresponding requests, and thus are not needed to set `req_seq_no` and `ret_code`. Each event message has its own purpose.

#### 2.2.1 MsgRegister

Mediapipe sends this message to manager when connection is established, to notify manager the pipeline id.

#### 2.2.1 MsgMetadata

Mediapipe sends this message to manager when OpenVINO metadata is generated.

