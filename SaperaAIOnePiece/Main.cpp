#include "CProAi.h"
#include "SapClassBasic.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <stdexcept>
#include <stdio.h>
using namespace std;

struct Result
{
    Result()
    {
        profilerTime = 0.0f;
    }

    struct ObjectInfo
    {
        ObjectInfo()
        {
            referenceClassIndex = 0;
            confidenceScore = 0.0f;
        }

        unsigned int referenceClassIndex;
        CProRect boundingBox;
        float confidenceScore;
    };

    CProImage proImage;
    std::vector<ObjectInfo> objectInfo;
    float profilerTime;
};

// Global Values Initialize
const auto OneGigabyte = 1024.0 * 1024.0 * 1024.0;
const auto Ratio = 512;
CProAiInferenceObjectDetection m_inferenceEngine;
CProAiInference::ModelAttributes m_modelAttributes;
vector <CProImage> splitImages;
vector <Result> splitImagesResults;
Result mergeReslut;

// Functions Initialize
int PaddingAndSplitCProImage(CProImage image);
int SaperaBufferToCProImage(SapBuffer &inBuf, CProImage& image);
int AI_ObjectDetection_Process(CProImage image);
int AI_ObjectDetection_Load(string modelFilename);
int AI_ObjectDetection_Build();



int main() {
    // Initial the Parameters.
    string modelFilename = ".//Heatsink_0.4.mod";
    string inputFileName ;
	string filePath ;
	string fileNumber;
	string readFileName;

	cout << "Please enter the file path: ";	
	cin >> filePath;
	
	shellCmd("dir /b /a-d " + filePath + "\\*.bmp | find /v /c\"\"", fileNumber);
	cout << fileNumber;
	
	shellCmd("dir /b /a-d "+ filePath +"\\*.bmp", readFileName);
	cout << readFileName;

	inputFileName = filePath + "\\"+ readFileName;
	cout << inputFileName;

    SapBuffer inBuf;
    CProImage image;
    int result = 0;

    // Allocate the image into Sapera Buffer.
    inBuf.SetParametersFromFile(inputFileName.c_str(), SapDefBufferType);

    // Synchronize the Sapera buffer and CProImage format.
    SapFormat format = inBuf.GetFormat();
    switch (format) {
    case SapFormatRGB888:
        inBuf.SetFormat(SapFormatRGB8888);
        break;
    default:
        break;
    }

    if (!inBuf.Create())
        return 0;
    // Load input image
    if (!inBuf.Load(inputFileName.c_str()))
        return 0;

    // Sapera to CProImage
    result += SaperaBufferToCProImage(inBuf, image);
    if (result == 0) {
        cout << "***Sapera Buffer to CProImage Buffer PASS.***" << endl << endl;
    }
    else {
        cout << "***Sapera Buffer to CProImage Buffer FAIL.***" << endl << endl;
        return 1;
    }

    // Padding and Split
    result += PaddingAndSplitCProImage(image);
    if (result == 0) {
        cout << "***Padding and Split CProImage into several SubImages PASS.***" << endl << endl;
    }
    else {
        cout << "***Padding and Split CProImage into several SubImages FAIL.***" << endl << endl;
        return 1;
    }
    
    // Load AI Object Detection
    result += AI_ObjectDetection_Load(modelFilename);
    if (result == 0) {
        cout << "***Load AI Object Detection PASS.***" << endl << endl;
    }
    else {
        cout << "***Load AI Object Detection FAIL.***" << endl << endl;
        return 1;
    }

    // Build AI Object Detection
    result += AI_ObjectDetection_Build();
    if (result == 0) {
        cout << "***Build AI Object Detection PASS.***" << endl << endl;
    }
    else {
        cout << "***Build AI Object Detection FAIL.***" << endl << endl;
        return 1;
    }

    int splitImageSize = splitImages.size();
    for (int i = 0; i < splitImageSize; i++) {
        // Process AI Object Detection
        result += AI_ObjectDetection_Process(splitImages.at(i));
        if (result == 0) {
            cout << "***Process '" << i <<  "' AI Object Detection PASS.***" << endl << endl;
        }
        else {
            cout << "***Process '" << i << "' AI Object Detection FAIL.***" << endl << endl;
            return 1;
        }
    }

    // Regarding the combination of split images.
    int width = mergeReslut.proImage.GetWidth();
    int height = mergeReslut.proImage.GetHeight();

    // Set the Drowing Parameters.
    void* mergePtr = mergeReslut.proImage.GetData();
    SapBuffer* mergeBuffer = new SapBuffer(1, &mergePtr, width, height,
        SapFormatRGB8888, SapBuffer::TypeScatterGather);
    SapGraphic* m_Graphic = new SapGraphic();
    //SapDataRGB color(255, 0, 0);
    SapDataMono color((255 << 16) | (0 << 8) | 0);
    m_Graphic->SetColor(color);
    m_Graphic->Create();
    mergeBuffer->Create();
    

    for (int i = 0; i < height / Ratio; i++) {
        for (int j = 0; j < width / Ratio; j++) {
            int TargetX = 0 + j * Ratio;
            int TargetY = 0 + i * Ratio;

            Result tempResult = splitImagesResults.at((width / Ratio) * i + j);
            for (int k = 0; k < tempResult.objectInfo.size(); k++) {
                Result::ObjectInfo tempInfo = tempResult.objectInfo.at(k);
                if (tempInfo.confidenceScore < 0.10)
                    continue;

                tempInfo.boundingBox.x += TargetX;
                tempInfo.boundingBox.y += TargetY;
                CProRect roi(tempInfo.boundingBox.x, tempInfo.boundingBox.y, tempInfo.boundingBox.w, tempInfo.boundingBox.h);
                
				m_Graphic->Rectangle(mergeBuffer, tempInfo.boundingBox.x, tempInfo.boundingBox.y, // Drawing
                    tempInfo.boundingBox.x + tempInfo.boundingBox.w, tempInfo.boundingBox.y + tempInfo.boundingBox.h);

				if (tempInfo.boundingBox.y - 18 <= 0)
				{
					m_Graphic->Text(mergeBuffer
						, tempInfo.boundingBox.x
						, tempInfo.boundingBox.y + tempInfo.boundingBox.h
						, m_modelAttributes.GetReferenceClassName(tempInfo.referenceClassIndex)); //Drawing Text
				}
				else
				{
					m_Graphic->Text(mergeBuffer
						, tempInfo.boundingBox.x
						, tempInfo.boundingBox.y - 18
						, m_modelAttributes.GetReferenceClassName(tempInfo.referenceClassIndex)); //Drawing Text
				}

				

               // CProImage tempRoi = CProImage(width, height, mergeReslut.proImage.GetFormat(), roi, mergeReslut.proImage.GetData());
                
     //           string saveFileName;
     //           saveFileName.assign(".//Output_Image//")
					//.append(m_modelAttributes.GetReferenceClassName(tempInfo.referenceClassIndex)) //Defect_Name
					//.append("_X-"+to_string(tempInfo.boundingBox.x))  //X position
					//.append("_Y-"+to_string(tempInfo.boundingBox.y))  //Y position
					//.append(".bmp");                                  // Image Format
     //           tempRoi.Save(saveFileName.c_str(), CProImage::FileBmp);
                mergeReslut.objectInfo.push_back(tempInfo);
            }
        }
    }

    SapView* View = new SapView(mergeBuffer, (HWND)-1);
	mergeBuffer->Save(".//Output_Image//InferenceResult.bmp", "-format bmp");  //Save Inference Result
    View->Create();
    View->Show();

    system("pause");
    return 0;
}

//#*****************************************************************************
//# Function   : shellCmd
//# Description : Read System command return result.
//# Inputs : System command
//# Outputs : return command result
//# Notice : None
//#*****************************************************************************
bool shellCmd(const string &cmd, string &result) {
	char buffer[512];
	result = "";

	// Open pipe to file
	FILE* pipe = _popen(cmd.c_str(), "r");
	if (!pipe) {
		return false;
	}

	// read till end of process:
	while (!feof(pipe)) {
		// use buffer to read and add to result
		if (fgets(buffer, sizeof(buffer), pipe) != NULL)
			result += buffer;
	}

	_pclose(pipe);
	return true;
}

//#*****************************************************************************
//# Function   : PaddingAndSplitCProImage
//# Description : Transfer Sapera Buffer to CProImage Buffer.
//# Inputs : SapBuffer &inBuf, CProImage &image
//# Outputs : Pass : return 0 ; Fail : return 1
//# Notice : None
//#*****************************************************************************
int PaddingAndSplitCProImage(CProImage image) {
    cout << "[ Start to Padding and Split CProImage ]" << endl;

    // Padding and Split
    int NewX = 0, NewY = 0;
    int nowX = image.GetWidth();
    int nowY = image.GetHeight();

    if (nowX % Ratio != 0) NewX = Ratio * (nowX / Ratio + 1);
	else NewX = nowX;
    if (nowY % Ratio != 0) NewY = Ratio * (nowY / Ratio + 1);
	else NewY = nowY;
    cout << "The X size change from " << nowX << " to " << NewX << endl;
    cout << "The Y size change from " << nowY << " to " << NewY << endl << endl;
    
    string FolderDir = ".//Split_Image//";
    for (int i = 0; i < NewY / Ratio; i++) {
        for (int j = 0; j < NewX / Ratio; j++) {
            string saveFileName;
            saveFileName.assign(FolderDir).append(to_string(i)).append(to_string(j)).append(".bmp");
            int TargetX = 0 + j * Ratio;
            int TargetY = 0 + i * Ratio;
            CProRect roi(TargetX, TargetY, Ratio, Ratio);
            CProImage paddingImage = CProImage(NewX, NewY, image.GetFormat());
            paddingImage.Copy(image);
            mergeReslut.proImage = paddingImage; // Save image for last result.

            CProImage tempImage = CProImage(NewX, NewY, image.GetFormat(), roi, paddingImage.GetData());
            CProImage roiImage = CProImage(Ratio, Ratio, tempImage.GetFormat());
            roiImage.Copy(tempImage);

            splitImages.push_back(roiImage);
            cout << "Start saving the divided image '" << saveFileName << "'." << endl;
            roiImage.Save(saveFileName.c_str(), CProImage::FileBmp);
        }
    }

    return 0;
}

//#*****************************************************************************
//# Function   : SaperaBufferToCProImage
//# Description : Transfer Sapera Buffer to CProImage Buffer.
//# Inputs : SapBuffer &inBuf, CProImage &image
//# Outputs : Pass : return 0 ; Fail : return 1
//# Notice : None
//#*****************************************************************************
int SaperaBufferToCProImage(SapBuffer &inBuf, CProImage &image) {

    try {
        void* data = NULL;
        int width = inBuf.GetWidth();
        int height = inBuf.GetHeight();
        inBuf.GetAddress((void**)&data);

        CProData::Format format; 
        switch (inBuf.GetFormat())
        {
        case SapFormatMono8:
            format = CProData::FormatUByte;
            break;
        case SapFormatMono16:
            format = CProData::FormatUShort;
            break;
        case SapFormatRGB8888:
            format = CProData::FormatRgb;
            break;
        default:
            format = CProData::FormatUnknown;
            break;
        };

        image = CProImage(width, height, format, data);
    }
    catch (invalid_argument& e) {
        cerr << " Transfer Sapera Buffer to CProImage Buffer FAIL by " << e.what() << endl;
        return 1;
    }
    return 0;
}

//#*****************************************************************************
//# Function   : AI_ObjectDetection_Process
//# Description : Use AI Object Detection to execute the target image.
//# Inputs : CProImage image
//# Outputs : Pass : return 0 ; Fail : return 1
//# Notice : None
//#*****************************************************************************
int AI_ObjectDetection_Process(CProImage image) {
    cout << "[ Start to process AI Object Detection ]" << endl;
    Result res;

    // Enable the profiler when doing the inference
    m_inferenceEngine.EnableProfiler(true);

    // Save input image
    res.proImage = std::move(image);

    // Save profiler time
    res.profilerTime = m_inferenceEngine.GetProfilerTimeInMillis(CProAiInference::ProfilerSection::All);

    
    // Do the inference on the input image
    if (!m_inferenceEngine.Execute(image, m_modelAttributes))
    {
        std::cout << "Fail to execute inference on the image" << std::endl;
        return 1;
    }


    if (m_inferenceEngine.IsProfilerEnabled())
    {
        std::cout << std::setw(6) << std::setprecision(2) << std::fixed << std::showpoint;
        std::cout << " PreProcessing: " << m_inferenceEngine.GetProfilerTimeInMillis(CProAiInference::ProfilerSection::PreProcessing) << " ms";
        std::cout << " Inference: " << m_inferenceEngine.GetProfilerTimeInMillis(CProAiInference::ProfilerSection::Inference) << " ms";
        std::cout << " PostProcesssing: " << m_inferenceEngine.GetProfilerTimeInMillis(CProAiInference::ProfilerSection::PostProcessing) << " ms";
        std::cout << std::endl;
    }

    // Get the inference's result for object detection
    auto& predictionResult = m_inferenceEngine.GetResult();
    auto objectCount = predictionResult.GetObjectCount();


    // Get and save the reference class index, the confidence score and the bounding box for each of them
    cout << endl << "Nunber of Object Count: " << objectCount << endl;
    cout << "-----------------------" << endl;
    for (unsigned int objectIndex = 0; objectIndex < objectCount; objectIndex++)
    {
        Result::ObjectInfo objectInfo;

        objectInfo.referenceClassIndex = predictionResult.GetObjectReferenceClassIndex(objectIndex);
        objectInfo.confidenceScore = predictionResult.GetObjectConfidenceScore(objectIndex);
        objectInfo.boundingBox = predictionResult.GetObjectBoundingBox(objectIndex);
        res.objectInfo.push_back(objectInfo);

        cout << "Reference Class Index: " << objectInfo.referenceClassIndex << endl;
        cout << "Confidence Score: " << objectInfo.confidenceScore << endl;
        cout << "Bounding Box : (" << objectInfo.boundingBox.x << ", " << objectInfo.boundingBox.y << 
            "), Width: " << objectInfo.boundingBox.w << " Height: " << objectInfo.boundingBox.h << endl << endl;
    }

    splitImagesResults.push_back(res);

    return 0;
}

//#*****************************************************************************
//# Function   : AI_ObjectDetection_Load
//# Description : Load AI Object Detection from the modelFilename (*.mod).
//# Inputs : string modelFilename
//# Outputs : Pass : return 0 ; Fail : return 1
//# Notice : None
//#*****************************************************************************
int AI_ObjectDetection_Load(string modelFilename) {
    cout << "[ Start to load AI Object Detection ]" << endl;

    string ModelFilename;
    auto modelAttributes = CProAiInference::GetModelAttributes(modelFilename.c_str());

    // Check Object Detection Model
    if (modelAttributes.GetModelType() == CProAiInference::ModelType::ObjectDetection)
        ModelFilename = modelFilename;
    else {
        cout << "This Model is not Object Detection Model !";
        return 1;
    }

    std::cout << "Loading the model ...";
    // Load the object detection model into the inference engine device
    if (!m_inferenceEngine.LoadModel(ModelFilename.c_str()))
    {
        std::cout << "Fail to load the model" << std::endl;
        return 1;
    }
    std::cout << " Done";
    std::cout << " ( " << m_inferenceEngine.GetProfilerTimeInMillis(CProAiInference::ProfilerSection::LoadModel) << " ms)" << std::endl;

    // Retrieve and display default values for object detection parameters
    int m_candidateNumberMax = m_inferenceEngine.GetCandidateNumberMax();
    float m_confidenceThreshold = m_inferenceEngine.GetConfidenceThreshold();
    float m_overlapThreshold = m_inferenceEngine.GetOverlapThreshold();

    std::cout << "Model Parameters" << std::endl;
    std::cout << " Candidate Number Max: " << m_candidateNumberMax;
    std::cout << " Confidence Threshold: " << m_confidenceThreshold;
    std::cout << " Overlap Threshold: " << m_overlapThreshold;
    std::cout << std::endl;
    std::cout << std::endl;

    // Retrieve the model attributes
    m_modelAttributes = m_inferenceEngine.GetModelAttributes();

    unsigned int inputSize = 0;
    if (inputSize == 0)
        inputSize = m_modelAttributes.GetInputSizeMax();

    if (inputSize < m_modelAttributes.GetInputSizeMin())
        inputSize = m_modelAttributes.GetInputSizeMin();

    if (inputSize > m_modelAttributes.GetInputSizeMax())
        inputSize = m_modelAttributes.GetInputSizeMax();

    m_modelAttributes.SetInputSize(inputSize);

    return 0;
}

//#*****************************************************************************
//# Function   : AI_ObjectDetection_Build
//# Description : Build AI Object Detection.
//# Inputs : None
//# Outputs : Pass : return 0 ; Fail : return 1
//# Notice : None
//#*****************************************************************************
int AI_ObjectDetection_Build() {
    cout << "[ Start to build AI Object Detection ]" << endl;

    std::cout << "Building the model ...";
    // Build/optimize the model
    if (!m_inferenceEngine.BuildModel(m_modelAttributes))
    {
        std::cout << "Fail to build the model" << std::endl;
        return 1;
    }
    std::cout << " Done";
    std::cout << " ( " << m_inferenceEngine.GetProfilerTimeInMillis(CProAiInference::ProfilerSection::BuildModel) << " ms)" << std::endl;

    std::cout << std::endl;

    std::cout << "Model Attributes" << std::endl;

    std::cout << "\tModel Type: " << static_cast<int>(m_modelAttributes.GetModelType()) << std::endl;
    std::cout << "\tModel Name: " << m_modelAttributes.GetModelName() << std::endl;

    std::cout << "\tInput Size: " << m_modelAttributes.GetInputSize() << std::endl;
    std::cout << "\tInput Channel Number: " << m_modelAttributes.GetInputChannelNumber() << std::endl;
    std::cout << "\tInput Resize Method: " << m_modelAttributes.GetInputResizeMethodName() << std::endl;
    std::cout << "\tInput Normalization: " << (m_modelAttributes.IsInputNormalized() ? "Yes" : "No") << std::endl;

    if (m_modelAttributes.GetMemoryUsageInference() > 0)
        std::cout << "\tMemory Usage Inference (estimation): " << m_modelAttributes.GetMemoryUsageInference() / OneGigabyte << " GB" << std::endl;

    auto referenceClassesCount = m_modelAttributes.GetReferenceClassCount();
    if (referenceClassesCount > 0)
        std::cout << "\tClass Number: " << referenceClassesCount << std::endl;

    for (auto index = 0u; index < referenceClassesCount; index++)
    {
        std::cout << "\t\tClass Index: " << index << " Name: " << m_modelAttributes.GetReferenceClassName(index) << std::endl;
    }

    std::cout << std::endl;

    return 0;
}
