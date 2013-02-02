/*
 * EMotion FX 2 - Character Animation System
 * Copyright (c) 2001-2004 Mystic Game Development - http://www.mysticgd.com
 * All Rights Reserved.
 */

// include some required headers
#include "ChunkProcessors.h"
#include "Importer.h"

#include "../Node.h"
#include "../Motion.h"
#include "../SkeletalMotion.h"
#include "../MotionPart.h"
#include "../Actor.h"
#include "../Mesh.h"
#include "../MeshDeformerStack.h"
#include "../SoftSkinDeformer.h"
#include "../SimpleMesh.h"
#include "../SubMesh.h"
#include "../Material.h"
#include "../StandardMaterial.h"
#include "../FXMaterial.h"
#include "../SkinningInfoVertexAttributeLayer.h"
#include "../SoftSkinManager.h"
#include "../LinearInterpolators.h"
#include "../UVVertexAttributeLayer.h"
#include "../NodeLimitAttribute.h"
#include "../NodePhysicsAttribute.h"
#include "../MeshExpressionPart.h"
#include "../FacialMotionPart.h"
#include "../FacialMotion.h"
#include "../NodeIDGenerator.h"
#include "../HermiteInterpolators.h"
#include "../SmartMeshMorphDeformer.h"

// use the Core namespace
using namespace MCore;


namespace EMotionFX
{

// constructor
ChunkProcessor::ChunkProcessor(Importer* lmaImporter, const int chunkID, const int version)
{
	mLMAImporter	= lmaImporter;
	mChunkID		= chunkID;
	mVersion		= version;
	mLoggingActive	= false;
}


// destructor
ChunkProcessor::~ChunkProcessor()
{
}


void ChunkProcessor::SetChunkID(const int chunkID)
{
	mChunkID = chunkID;
}


void ChunkProcessor::SetVersion(const int versionNumber)
{
	mVersion = versionNumber;
}


int ChunkProcessor::GetChunkID() const
{
	return mChunkID;
}


int ChunkProcessor::GetVersion() const
{
	return mVersion;
}


void ChunkProcessor::SetLogging(const bool loggingActive)
{
	mLoggingActive = loggingActive;
}


bool ChunkProcessor::GetLogging() const
{
	return mLoggingActive;
}


//=================================================================================================


// constructor
NodeChunkProcessor1::NodeChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_NODE, 1)
{
}


// destructor
NodeChunkProcessor1::~NodeChunkProcessor1()
{
}


bool NodeChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");
		return false;
	}

	// read the node header
	file->Read(&mNodeHeader, sizeof(LMA_Node));

	if (GetLogging()) LOG("- Node name = '%s'", mNodeHeader.mName);
	if (GetLogging()) LOG("    + Parent = '%s'", mNodeHeader.mParent);
	if (GetLogging()) LOG("    + Position:    x=%f, y=%f, z=%f", (float)mNodeHeader.mLocalPos.mX, (float)mNodeHeader.mLocalPos.mY, (float)mNodeHeader.mLocalPos.mZ);
	if (GetLogging()) LOG("    + Orientation: x=%f, y=%f, z=%f, w=%f", (float)mNodeHeader.mLocalQuat.mX, (float)mNodeHeader.mLocalQuat.mY, (float)mNodeHeader.mLocalQuat.mZ, (float)mNodeHeader.mLocalQuat.mW);
	if (GetLogging()) LOG("    + Scale:       x=%f, y=%f, z=%f", (float)mNodeHeader.mLocalScale.mX, (float)mNodeHeader.mLocalScale.mY, (float)mNodeHeader.mLocalScale.mZ);
	if (GetLogging()) LOG("    + Inverse Matrix: %f, \t%f, \t%f, \t%f", (float)mNodeHeader.mInvBoneTM.m[0], (float)mNodeHeader.mInvBoneTM.m[1], (float)mNodeHeader.mInvBoneTM.m[2], (float)mNodeHeader.mInvBoneTM.m[3]);
	if (GetLogging()) LOG("                      %f, \t%f, \t%f, \t%f", (float)mNodeHeader.mInvBoneTM.m[4], (float)mNodeHeader.mInvBoneTM.m[5], (float)mNodeHeader.mInvBoneTM.m[6], (float)mNodeHeader.mInvBoneTM.m[7]);
	if (GetLogging()) LOG("                      %f, \t%f, \t%f, \t%f", (float)mNodeHeader.mInvBoneTM.m[8], (float)mNodeHeader.mInvBoneTM.m[9], (float)mNodeHeader.mInvBoneTM.m[10],(float)mNodeHeader.mInvBoneTM.m[11]);
	if (GetLogging()) LOG("                      %f, \t%f, \t%f, \t%f", (float)mNodeHeader.mInvBoneTM.m[12],(float)mNodeHeader.mInvBoneTM.m[13],(float)mNodeHeader.mInvBoneTM.m[14],(float)mNodeHeader.mInvBoneTM.m[15]);


	// create the new node
	Node* node = new Node(mNodeHeader.mName);

	// add it to the actor
	actor->AddNode(node);

	// set the transformation
	node->SetLocalPos	( Vector3(mNodeHeader.mLocalPos.mX,		mNodeHeader.mLocalPos.mY,	mNodeHeader.mLocalPos.mZ) );
	node->SetLocalScale	( Vector3(mNodeHeader.mLocalScale.mX,	mNodeHeader.mLocalScale.mY, mNodeHeader.mLocalScale.mZ) );
	node->SetLocalRot	( Quaternion(mNodeHeader.mLocalQuat.mX,	mNodeHeader.mLocalQuat.mY,	mNodeHeader.mLocalQuat.mZ, mNodeHeader.mLocalQuat.mW) );
	node->UpdateLocalTM();

	node->SetOrgPos( node->GetLocalPos() );
	node->SetOrgRot( node->GetLocalRot() );
	node->SetOrgScale( node->GetLocalScale() );

	// set the inverse world space matrix
	Matrix mat(true);
	mat.m44[0][0]=mNodeHeader.mInvBoneTM.m[0+0*4];	mat.m44[0][1]=mNodeHeader.mInvBoneTM.m[1+0*4];	mat.m44[0][2]=mNodeHeader.mInvBoneTM.m[2+0*4];//	mat.m44[0][2]=mNodeHeader.mInvBoneTM.m[3+0*4];
	mat.m44[1][0]=mNodeHeader.mInvBoneTM.m[0+1*4];	mat.m44[1][1]=mNodeHeader.mInvBoneTM.m[1+1*4];	mat.m44[1][2]=mNodeHeader.mInvBoneTM.m[2+1*4];//	mat.m44[1][2]=mNodeHeader.mInvBoneTM.m[3+1*4];
	mat.m44[2][0]=mNodeHeader.mInvBoneTM.m[0+2*4];	mat.m44[2][1]=mNodeHeader.mInvBoneTM.m[1+2*4];	mat.m44[2][2]=mNodeHeader.mInvBoneTM.m[2+2*4];//	mat.m44[2][2]=mNodeHeader.mInvBoneTM.m[3+2*4];
	mat.m44[3][0]=mNodeHeader.mInvBoneTM.m[0+3*4];	mat.m44[3][1]=mNodeHeader.mInvBoneTM.m[1+3*4];	mat.m44[3][2]=mNodeHeader.mInvBoneTM.m[2+3*4];//	mat.m44[3][2]=mNodeHeader.mInvBoneTM.m[3+3*4];
	node->SetInvBoneTM(mat);

	// add a parent link
	if (mNodeHeader.mParent[0] != '\0')
		AddLink(node, mNodeHeader.mParent);

	// get the shared data
	Importer::SharedHierarchyInfo* sharedHierarchyInfo = (Importer::SharedHierarchyInfo*)mLMAImporter->FindSharedData( Importer::SharedHierarchyInfo::TYPE_ID );

	// if we haven't found it, something is wrong in the implementation of the shared data or this processor
	assert( sharedHierarchyInfo );

	// if we found it, add it
	if (sharedHierarchyInfo)
	{
		sharedHierarchyInfo->mNodes.Add(node);
		sharedHierarchyInfo->mLastNode = node;
	}
	else
	{
		delete node;
		if (GetLogging())
			LOG("Can't find shared data.");
		return false;
	}

	return true;
}


// create a node/parent link
void NodeChunkProcessor1::AddLink(Node* node, char* parent)
{
	DECLARE_FUNCTION(AddLink)

	// create a new link
	Importer::SharedHierarchyInfo::Link link;
	link.mNode		= node;
	link.mParent	= parent;

	// get the shared data
	Importer::SharedHierarchyInfo* sharedHierarchyInfo = (Importer::SharedHierarchyInfo*)mLMAImporter->FindSharedData( Importer::SharedHierarchyInfo::TYPE_ID );

	if (sharedHierarchyInfo)
		sharedHierarchyInfo->mLinks.Add(link);
	else
	{
		if (GetLogging())
			LOG("Can't find shared data.");
	}
}


//=================================================================================================


// constructor
MeshChunkProcessor1::MeshChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MESH, 1)
{
}


// destructor
MeshChunkProcessor1::~MeshChunkProcessor1()
{
}


bool MeshChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		LOG_ERROR("Passed actor seems to be not valid!");
		return false;
	}

	// read the mesh header
	LMA_Mesh meshHeader;
	file->Read(&meshHeader, sizeof(LMA_Mesh));

	if (GetLogging()) LOG("- Mesh");
	if (GetLogging()) LOG("    + NodeNumber    = %d", meshHeader.mNodeNumber);
	if (GetLogging()) LOG("    + NumOrgVerts   = %d", meshHeader.mNumOrgVerts);
	if (GetLogging()) LOG("    + NumVerts      = %d", meshHeader.mTotalVerts);
	if (GetLogging()) LOG("    + NumIndices    = %d", meshHeader.mTotalIndices);
	if (GetLogging()) LOG("    + NumSubMeshes  = %d", meshHeader.mNumSubMeshes);

	// create the smartpointer to the mesh object
	// we use a smartpointer here, because of the reference counting system needed for shared meshes.
	MCore::Pointer<Mesh> mesh( new Mesh(meshHeader.mTotalVerts, meshHeader.mTotalIndices/3, meshHeader.mNumOrgVerts) );

	// link the mesh to the correct node
	actor->GetNode( meshHeader.mNodeNumber )->SetMesh( mesh, 0 );

	LMA_SubMesh subMesh;
	LMA_SubMeshVertex vertex;
	int index;

	// add an uv layer
	const int numVerts = mesh->GetNumVertices();
	UVVertexAttributeLayer* uvLayer = new UVVertexAttributeLayer(numVerts);
	mesh->AddVertexAttributeLayer( uvLayer );

	// get the arrays
	Vector3* positions	= mesh->GetPositions();
	Vector3* normals	= mesh->GetNormals();
	Vector3* orgPos		= mesh->GetOrgPositions();
	Vector3* orgNormals = mesh->GetOrgNormals();
	int*	 orgVerts	= mesh->GetOrgVerts();
	int*	 indices	= mesh->GetIndices();
	Vector2* uvData		= uvLayer->GetUVs();

	int vertexOffset	= 0;
	int indexOffset		= 0;
	int startVertex		= 0;

	// read all submeshes
	for (int i=0; i<meshHeader.mNumSubMeshes; i++)
	{
		// read the submesh header
		file->Read(&subMesh, sizeof(LMA_SubMesh));

		if (GetLogging()) LOG("    - Reading SubMesh");
		if (GetLogging()) LOG("       + Material ID   = %d", subMesh.mMatID);
		if (GetLogging()) LOG("       + NumOfIndices  = %d", subMesh.mNumIndices);
		if (GetLogging()) LOG("       + NumOfVertices = %d", subMesh.mNumVerts);
		if (GetLogging()) LOG("       + NumOfUVSets   = %d", subMesh.mNumUVSets);


		// create and add the submesh
		SubMesh *lmSubMesh = new SubMesh(mesh, vertexOffset, indexOffset, subMesh.mNumVerts, subMesh.mNumIndices, subMesh.mMatID);
		mesh->AddSubMesh( lmSubMesh );

		// read the vertices
		for (int v=0; v<subMesh.mNumVerts; v++)
		{
			// read the vertex
			file->Read(&vertex, sizeof(LMA_SubMeshVertex));

			// set the vertex data
			positions[vertexOffset] = Vector3(vertex.mPos.mX, vertex.mPos.mY, vertex.mPos.mZ);
			normals[vertexOffset]	= Vector3(vertex.mNormal.mX, vertex.mNormal.mY, vertex.mNormal.mZ);
			orgPos[vertexOffset]	= positions[vertexOffset];
			orgNormals[vertexOffset]= normals[vertexOffset];
			orgVerts[vertexOffset]	= vertex.mOrgVtx;
			uvData[vertexOffset]	= Vector2( vertex.mUV.mU, vertex.mUV.mV );

			// next vertex in the big mesh
			vertexOffset++;
		}

		// read the indices
		for (int a=0; a<subMesh.mNumIndices; a++)
		{
			file->Read(&index, sizeof(int));
			indices[indexOffset] = startVertex + index;
			indexOffset++;
		}

		startVertex += subMesh.mNumVerts;
	}

	// calculate the tangents
	if (usePerPixelLighting)
		mesh->CalcTangents();

	return true;
}



//=================================================================================================


// constructor
MeshChunkProcessor2::MeshChunkProcessor2(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MESH, 2)
{
}


// destructor
MeshChunkProcessor2::~MeshChunkProcessor2()
{
}


bool MeshChunkProcessor2::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		LOG_ERROR("Passed actor seems to be not valid!");
		return false;
	}

	// read the mesh header
	LMA_Mesh2 meshHeader;
	file->Read(&meshHeader, sizeof(LMA_Mesh2));

	if (GetLogging()) LOG("- Mesh");
	if (GetLogging()) LOG("    + NodeNumber      = %d", meshHeader.mNodeNumber);
	if (GetLogging()) LOG("    + NumOrgVerts     = %d", meshHeader.mNumOrgVerts);
	if (GetLogging()) LOG("    + NumVerts        = %d", meshHeader.mTotalVerts);
	if (GetLogging()) LOG("    + NumIndices      = %d", meshHeader.mTotalIndices);
	if (GetLogging()) LOG("    + NumSubMeshes    = %d", meshHeader.mNumSubMeshes);
	if (GetLogging()) LOG("    + IsCollisionMesh = %d", meshHeader.mIsCollisionMesh);

	// create the smartpointer to the mesh object
	// we use a smartpointer here, because of the reference counting system needed for shared meshes.
	MCore::Pointer<Mesh> mesh( new Mesh(meshHeader.mTotalVerts, meshHeader.mTotalIndices/3, meshHeader.mNumOrgVerts) );

	// link the mesh to the correct node
	if (meshHeader.mIsCollisionMesh)
		actor->GetNode( meshHeader.mNodeNumber )->SetCollisionMesh( mesh, 0 );
	else
		actor->GetNode( meshHeader.mNodeNumber )->SetMesh( mesh, 0 );

	LMA_SubMesh subMesh;
	LMA_SubMeshVertex vertex;
	int index;

	// add an uv layer
	UVVertexAttributeLayer* uvLayer = new UVVertexAttributeLayer( mesh->GetNumVertices() );
	mesh->AddVertexAttributeLayer( uvLayer );

	// get the arrays
	Vector3* positions	= mesh->GetPositions();
	Vector3* normals	= mesh->GetNormals();
	Vector3* orgPos		= mesh->GetOrgPositions();
	Vector3* orgNormals = mesh->GetOrgNormals();
	int*	 orgVerts	= mesh->GetOrgVerts();
	int*	 indices	= mesh->GetIndices();
	Vector2* uvData		= uvLayer->GetUVs();

	int vertexOffset	= 0;
	int indexOffset		= 0;
	int startVertex		= 0;

	// read all submeshes
	for (int i=0; i<meshHeader.mNumSubMeshes; i++)
	{
		// read the submesh header
		file->Read(&subMesh, sizeof(LMA_SubMesh));

		if (GetLogging()) LOG("    - Reading SubMesh");
		if (GetLogging()) LOG("       + Material ID   = %d", subMesh.mMatID);
		if (GetLogging()) LOG("       + NumOfIndices  = %d", subMesh.mNumIndices);
		if (GetLogging()) LOG("       + NumOfVertices = %d", subMesh.mNumVerts);
		if (GetLogging()) LOG("       + NumOfUVSets   = %d", subMesh.mNumUVSets);


		// create and add the submesh
		SubMesh* lmSubMesh = new SubMesh(mesh, vertexOffset, indexOffset, subMesh.mNumVerts, subMesh.mNumIndices, subMesh.mMatID);
		mesh->AddSubMesh( lmSubMesh );

		// read the vertices
		for (int v=0; v<subMesh.mNumVerts; v++)
		{
			// read the vertex
			file->Read(&vertex, sizeof(LMA_SubMeshVertex));

			// set the vertex data
			positions[vertexOffset] = Vector3(vertex.mPos.mX, vertex.mPos.mY, vertex.mPos.mZ);
			normals[vertexOffset]	= Vector3(vertex.mNormal.mX, vertex.mNormal.mY, vertex.mNormal.mZ);
			orgPos[vertexOffset]	= positions[vertexOffset];
			orgNormals[vertexOffset]= normals[vertexOffset];
			orgVerts[vertexOffset]	= vertex.mOrgVtx;
			uvData[vertexOffset]	= Vector2( vertex.mUV.mU, vertex.mUV.mV );

			// next vertex in the big mesh
			vertexOffset++;
		}

		// read the indices
		for (int a=0; a<subMesh.mNumIndices; a++)
		{
			file->Read(&index, sizeof(int));
			indices[indexOffset] = startVertex + index;
			indexOffset++;
		}

		startVertex += subMesh.mNumVerts;
	}

	// calculate the tangents
	if (usePerPixelLighting)
		mesh->CalcTangents();

	return true;
}



//=================================================================================================


// constructor
MeshChunkProcessor3::MeshChunkProcessor3(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MESH, 3)
{
}


// destructor
MeshChunkProcessor3::~MeshChunkProcessor3()
{
}


bool MeshChunkProcessor3::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		LOG_ERROR("Passed actor seems to be not valid!");
		return false;
	}

	// read the mesh header
	LMA_Mesh3 meshHeader;
	file->Read(&meshHeader, sizeof(LMA_Mesh3));

	if (GetLogging()) LOG("- Mesh");
	if (GetLogging()) LOG("    + NodeNumber      = %d", meshHeader.mNodeNumber);
	if (GetLogging()) LOG("    + NumOrgVerts     = %d", meshHeader.mNumOrgVerts);
	if (GetLogging()) LOG("    + NumVerts        = %d", meshHeader.mTotalVerts);
	if (GetLogging()) LOG("    + NumIndices      = %d", meshHeader.mTotalIndices);
	if (GetLogging()) LOG("    + NumSubMeshes    = %d", meshHeader.mNumSubMeshes);
	if (GetLogging()) LOG("    + NumUVSets       = %d", meshHeader.mNumUVSets);
	if (GetLogging()) LOG("    + IsCollisionMesh = %d", meshHeader.mIsCollisionMesh);

	// create the smartpointer to the mesh object
	// we use a smartpointer here, because of the reference counting system needed for shared meshes.
	MCore::Pointer<Mesh> mesh( new Mesh(meshHeader.mTotalVerts, meshHeader.mTotalIndices/3, meshHeader.mNumOrgVerts) );

	// link the mesh to the correct node
	if (meshHeader.mIsCollisionMesh)
		actor->GetNode( meshHeader.mNodeNumber )->SetCollisionMesh( mesh, 0 );
	else
		actor->GetNode( meshHeader.mNodeNumber )->SetMesh( mesh, 0 );

	LMA_SubMesh subMesh;
	LMA_SubMeshVertex2 vertex;
	int index;

	// add an uv layer
	Array<UVVertexAttributeLayer*> uvLayers;
	Array<Vector2*> uvDatas;
	int a;
	for (a=0; a<meshHeader.mNumUVSets; a++)
	{
		UVVertexAttributeLayer* layer = new UVVertexAttributeLayer( mesh->GetNumVertices() );
		mesh->AddVertexAttributeLayer( layer );
		uvLayers.Add(layer);
		uvDatas.Add(layer->GetUVs());
	}

	// get the arrays
	Vector3* positions	= mesh->GetPositions();
	Vector3* normals	= mesh->GetNormals();
	Vector3* orgPos		= mesh->GetOrgPositions();
	Vector3* orgNormals = mesh->GetOrgNormals();
	int*	 orgVerts	= mesh->GetOrgVerts();
	int*	 indices	= mesh->GetIndices();

	int vertexOffset	= 0;
	int indexOffset		= 0;
	int startVertex		= 0;

	// read all submeshes
	for (int i=0; i<meshHeader.mNumSubMeshes; i++)
	{
		// read the submesh header
		file->Read(&subMesh, sizeof(LMA_SubMesh));

		if (GetLogging()) LOG("    - Reading SubMesh");
		if (GetLogging()) LOG("       + Material ID   = %d", subMesh.mMatID);
		if (GetLogging()) LOG("       + NumOfIndices  = %d", subMesh.mNumIndices);
		if (GetLogging()) LOG("       + NumOfVertices = %d", subMesh.mNumVerts);

		// create and add the submesh
		SubMesh* lmSubMesh = new SubMesh(mesh, vertexOffset, indexOffset, subMesh.mNumVerts, subMesh.mNumIndices, subMesh.mMatID);
		mesh->AddSubMesh( lmSubMesh );

		// read the vertices
		for (int v=0; v<subMesh.mNumVerts; v++)
		{
			// read the vertex
			file->Read(&vertex, sizeof(LMA_SubMeshVertex2));

			// set the vertex data
			positions[vertexOffset] = Vector3(vertex.mPos.mX, vertex.mPos.mY, vertex.mPos.mZ);
			normals[vertexOffset]	= Vector3(vertex.mNormal.mX, vertex.mNormal.mY, vertex.mNormal.mZ);
			orgPos[vertexOffset]	= positions[vertexOffset];
			orgNormals[vertexOffset]= normals[vertexOffset];
			orgVerts[vertexOffset]	= vertex.mOrgVtx;

			// read the UVs
			LMA_UV uv;
			for (a=0; a<meshHeader.mNumUVSets; a++)
			{
				file->Read(&uv, sizeof(LMA_UV));
				uvDatas[a][vertexOffset] = Vector2( uv.mU, uv.mV );
			}

			// next vertex in the big mesh
			vertexOffset++;
		}

		// read the indices
		for (int a=0; a<subMesh.mNumIndices; a++)
		{
			file->Read(&index, sizeof(int));
			indices[indexOffset] = startVertex + index;
			indexOffset++;
		}

		startVertex += subMesh.mNumVerts;
	}

	// calculate the tangents
	if (usePerPixelLighting)
		mesh->CalcTangents();

	return true;
}

//=================================================================================================


// constructor
MeshChunkProcessor10000::MeshChunkProcessor10000(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MESH, 10000)
{
}


// destructor
MeshChunkProcessor10000::~MeshChunkProcessor10000()
{
}


bool MeshChunkProcessor10000::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)
 
	if (!actor)
	{ 
		LOG_ERROR("Passed actor seems to be not valid!");
		return false;
	}

	// read the mesh header
	LMA_Mesh10000 meshHeader;
	file->Read(&meshHeader, sizeof(LMA_Mesh10000));

	if (GetLogging()) LOG("- Mesh");
	if (GetLogging()) LOG("    + NodeNumber      = %d", meshHeader.mNodeNumber);
	if (GetLogging()) LOG("    + NumOrgVerts     = %d", meshHeader.mNumOrgVerts);
	if (GetLogging()) LOG("    + NumVerts        = %d", meshHeader.mTotalVerts);
	if (GetLogging()) LOG("    + NumIndices      = %d", meshHeader.mTotalIndices);
	if (GetLogging()) LOG("    + NumSubMeshes    = %d", meshHeader.mNumSubMeshes);
	if (GetLogging()) LOG("    + IsCollisionMesh = %d", meshHeader.mIsCollisionMesh);

	// create the smartpointer to the mesh object
	// we use a smartpointer here, because of the reference counting system needed for shared meshes.
	MCore::Pointer<Mesh> mesh( new Mesh(meshHeader.mTotalVerts, meshHeader.mTotalIndices/3, meshHeader.mNumOrgVerts) );

	// link the mesh to the correct node
	if (meshHeader.mIsCollisionMesh)
		actor->GetNode( meshHeader.mNodeNumber )->SetCollisionMesh( mesh, 0 );
	else
		actor->GetNode( meshHeader.mNodeNumber )->SetMesh( mesh, 0 );

	LMA_SubMesh subMesh;
	LMA_SubMeshVertex vertex;
	LMA_SubMeshVertex10000 vertex10000;
	int index;

	// add an uv layer
	UVVertexAttributeLayer* uvData = new UVVertexAttributeLayer( mesh->GetNumVertices() );
	UVVertexAttributeLayer * uvData1 = NULL;
	mesh->AddVertexAttributeLayer( uvData );
	if(strcmp(meshHeader.mShdMapName, "")!=0) {
		uvData1 = new UVVertexAttributeLayer( mesh->GetNumVertices() );
		mesh->AddVertexAttributeLayer( uvData1 );
		
		mesh->SetShdMapName(meshHeader.mShdMapName);
	}

	// get the arrays
	Vector3* positions	= mesh->GetPositions();
	Vector3* normals	= mesh->GetNormals();
	Vector3* orgPos		= mesh->GetOrgPositions();
	Vector3* orgNormals = mesh->GetOrgNormals();
	int*	 orgVerts	= mesh->GetOrgVerts();
	int*	 indices	= mesh->GetIndices();

	int vertexOffset	= 0;
	int indexOffset		= 0;
	int startVertex		= 0;

	// read all submeshes
	for (int i=0; i<meshHeader.mNumSubMeshes; i++)
	{
		// read the submesh header
		file->Read(&subMesh, sizeof(LMA_SubMesh));

		if (GetLogging()) LOG("    - Reading SubMesh");
		if (GetLogging()) LOG("       + Material ID   = %d", subMesh.mMatID);
		if (GetLogging()) LOG("       + NumOfIndices  = %d", subMesh.mNumIndices);
		if (GetLogging()) LOG("       + NumOfVertices = %d", subMesh.mNumVerts);
		if (GetLogging()) LOG("       + NumOfUVSets   = %d", subMesh.mNumUVSets);


		// create and add the submesh
		SubMesh* lmSubMesh = new SubMesh(mesh, vertexOffset, indexOffset, subMesh.mNumVerts, subMesh.mNumIndices, subMesh.mMatID);
		mesh->AddSubMesh( lmSubMesh );

		// read the vertices
		if(strcmp(meshHeader.mShdMapName, "")!=0) {
			for (int v=0; v<subMesh.mNumVerts; v++)
			{
				// read the vertex
				file->Read(&vertex10000, sizeof(LMA_SubMeshVertex10000));

				// set the vertex data
				positions[vertexOffset] = Vector3(vertex10000.mPos.mX, vertex10000.mPos.mY, vertex10000.mPos.mZ);
				normals[vertexOffset]	= Vector3(vertex10000.mNormal.mX, vertex10000.mNormal.mY, vertex10000.mNormal.mZ);
				orgPos[vertexOffset]	= positions[vertexOffset];
				orgNormals[vertexOffset]= normals[vertexOffset];
				orgVerts[vertexOffset]	= vertex10000.mOrgVtx;
				uvData->GetUVs()[vertexOffset] = Vector2( vertex10000.mUV0.mU, vertex10000.mUV0.mV );
				uvData1->GetUVs()[vertexOffset] = Vector2( vertex10000.mUV1.mU, vertex10000.mUV1.mV );

				// next vertex in the big mesh
				vertexOffset++;
			}
		} else {
			for (int v=0; v<subMesh.mNumVerts; v++)
			{
				// read the vertex
				file->Read(&vertex, sizeof(LMA_SubMeshVertex));
				
				// set the vertex data
				positions[vertexOffset] = Vector3(vertex.mPos.mX, vertex.mPos.mY, vertex.mPos.mZ);
				normals[vertexOffset]	= Vector3(vertex.mNormal.mX, vertex.mNormal.mY, vertex.mNormal.mZ);
				orgPos[vertexOffset]	= positions[vertexOffset];
				orgNormals[vertexOffset]= normals[vertexOffset];
				orgVerts[vertexOffset]	= vertex.mOrgVtx;
				uvData->GetUVs()[vertexOffset] = Vector2( vertex.mUV.mU, vertex.mUV.mV );

				// next vertex in the big mesh
				vertexOffset++;
			}			
		}

		// read the indices
		for (int a=0; a<subMesh.mNumIndices; a++)
		{
			file->Read(&index, sizeof(int));
			indices[indexOffset] = startVertex + index;
			indexOffset++;
		}

		startVertex += subMesh.mNumVerts;
	}

	// calculate the tangents
	if (usePerPixelLighting)
		mesh->CalcTangents();

	return true;
}





//=================================================================================================


// constructor
SkinningInfoChunkProcessor1::SkinningInfoChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_SKINNINGINFO, 1)
{
}


// destructor
SkinningInfoChunkProcessor1::~SkinningInfoChunkProcessor1()
{
}


bool SkinningInfoChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging()) LOG("Passed actor seems to be not valid!");
		return false;
	}

	if (GetLogging()) LOG("- Skinning Information");

	// read the node number this info belongs to
	int nodeNr;
	file->Read(&nodeNr, sizeof(int));

	if (GetLogging()) LOG("   + Node number = %d", nodeNr);
	if (GetLogging()) LOG("   + Node name   = %s", actor->GetNode(nodeNr)->GetNamePtr());

	// get the mesh of the node
	bool colMesh = false;
	if (actor->GetNode(nodeNr)->GetCollisionMesh(0).GetPointer())
		colMesh = true;

	Mesh* mesh;
	if (colMesh)
		mesh = actor->GetNode(nodeNr)->GetCollisionMesh( 0 ).GetPointer();
	else
		mesh = actor->GetNode(nodeNr)->GetMesh( 0 ).GetPointer();

	//if (GetLogging()) LOG("   + Mesh pointer = 0x%x", mesh);

	// add the skinning info to the mesh
	SkinningInfoVertexAttributeLayer* skinningLayer = new SkinningInfoVertexAttributeLayer( mesh->GetNumOrgVertices() );
	mesh->AddSharedVertexAttributeLayer( skinningLayer );

	if (GetLogging()) LOG("   + Num org vertices = %d", mesh->GetNumOrgVertices());

	// read all the influences, for each vertex
	LMA_SkinInfluence influence;
	const int numOrgVerts = mesh->GetNumOrgVertices();
	for (int i=0; i<numOrgVerts; i++)
	{
		// read the number of influences for this vertex
		char numInfluences;
		file->Read(&numInfluences, 1);

		//if (GetLogging()) LOG("   + Num influences for vertex #%d = %d", i, numInfluences);

		// load the influences
		for (int w=0; w<numInfluences; w++)
		{
			file->Read(&influence, sizeof(LMA_SkinInfluence));
			//if (GetLogging()) LOG("     + %d - nodeNr = %d    - weight = %f", i, influence.mNodeNr, influence.mWeight);

			// add the influence to the skinning attribute
			skinningLayer->AddInfluence(i, actor->GetNode(influence.mNodeNr), influence.mWeight);
		}
	}

	// get the stack
	Node *node = actor->GetNode(nodeNr);

	if (colMesh)
	{
		MCore::Pointer<MeshDeformerStack>& stack = node->GetCollisionMeshDeformerStack(0);

		// create the stack if it doesn't yet exist
		if (stack.GetPointer() == NULL)
		{
			stack = MCore::Pointer<MeshDeformerStack>( new MeshDeformerStack( node->GetCollisionMesh(0) ) );
			node->SetCollisionMeshDeformerStack(stack, 0);
		}

		// add the skinning deformer to the stack
		SoftSkinDeformer* skinDeformer = SoftSkinManager::GetInstance().CreateDeformer( node->GetCollisionMesh(0) );
		stack->AddDeformer( skinDeformer );
	}
	else
	{
		MCore::Pointer<MeshDeformerStack>& stack = node->GetMeshDeformerStack(0);

		// create the stack if it doesn't yet exist
		if (stack.GetPointer() == NULL)
		{
			stack = MCore::Pointer<MeshDeformerStack>( new MeshDeformerStack( node->GetMesh(0) ) );
			node->SetMeshDeformerStack(stack, 0);
		}

		// add the skinning deformer to the stack
		SoftSkinDeformer* skinDeformer = SoftSkinManager::GetInstance().CreateDeformer( node->GetMesh(0) );
		stack->AddDeformer( skinDeformer );
	}

	return true;
}

//=================================================================================================
/*
// constructor
CollisionMeshChunkProcessor1::CollisionMeshChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_COLLISIONMESH, 1)
{
}


// destructor
CollisionMeshChunkProcessor1::~CollisionMeshChunkProcessor1()
{
}


bool CollisionMeshChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");
		return false;
	}

	if (GetLogging()) LOG("- Collision Mesh");

	LMA_CollisionMesh obj;
	if (!file->Read(&obj, sizeof(LMA_CollisionMesh))) return false;

	if (GetLogging()) LOG("    + NumVertices = %d", obj.mNumVertices);
	if (GetLogging()) LOG("    + NumFaces    = %d", obj.mNumFaces);

	// create the mesh
	//SimpleMesh *mesh = new SimpleMesh(obj.mNumVertices, obj.mNumFaces);
	MCore::Pointer<SimpleMesh> mesh( new SimpleMesh(obj.mNumVertices, obj.mNumFaces) );

	// get pointers to the mesh data
	Vector3*	positions	= mesh->GetPositions();
	int*		indices		= mesh->GetIndices();

	// read the vertices
	LMA_Vector3 vertex;
	int i;
	for (i=0; i<obj.mNumVertices; i++)
	{
		file->Read(&vertex, sizeof(LMA_Vector3));
		positions[i].Set(vertex.mX, vertex.mY, vertex.mZ);
		mesh->GetBoundingBox().Encapsulate( positions[i] );
	}

	// read the faces
	int indexA, indexB, indexC, curIndex=0;
	for (i=0; i<obj.mNumFaces; i++)
	{
		file->Read(&indexA, sizeof(int));
		file->Read(&indexB, sizeof(int));
		file->Read(&indexC, sizeof(int));
		indices[ curIndex++ ] = indexA;
		indices[ curIndex++ ] = indexB;
		indices[ curIndex++ ] = indexC;
	}

	// set the collision mesh
	Importer::SharedHierarchyInfo* sharedHierarchyInfo = (Importer::SharedHierarchyInfo*)mLMAImporter->FindSharedData(SHAREDDATA_HIERARCHYINFO);

	if (sharedHierarchyInfo)
	{
		if (sharedHierarchyInfo->mLastNode != NULL)
			sharedHierarchyInfo->mLastNode->SetCollisionMesh( mesh );
	}
	else
	{
		if (GetLogging())
			LOG("Can't find shared data.");
		return false;
	}

	// update the boundingsphere
	mesh->GetBoundingSphere() = BoundingSphere( mesh->GetBoundingBox().CalcMiddle(), 0 );
	mesh->GetBoundingSphere().Encapsulate( mesh->GetBoundingBox().GetMin() );
	mesh->GetBoundingSphere().Encapsulate( mesh->GetBoundingBox().GetMax() );

	return true;
}
*/

//=================================================================================================


// constructor
MaterialChunkProcessor1::MaterialChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MATERIAL, 1)
{
}


// destructor
MaterialChunkProcessor1::~MaterialChunkProcessor1()
{
}


bool MaterialChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");

		return false;
	}

	LMA_Material material;
	file->Read(&material, sizeof(LMA_Material));

	// print material information
	if (GetLogging()) LOG("- Material name = '%s'", material.mName);
	if (GetLogging()) LOG("    + Ambient       : (%f, %f, %f)", material.mAmbient.mR,  material.mAmbient.mG,  material.mAmbient.mB);
	if (GetLogging()) LOG("    + Diffuse       : (%f, %f, %f)", material.mDiffuse.mR,  material.mDiffuse.mG,  material.mDiffuse.mB);
	if (GetLogging()) LOG("    + Specular      : (%f, %f, %f)", material.mSpecular.mR, material.mSpecular.mG, material.mSpecular.mB);
	if (GetLogging()) LOG("    + Emissive      : (%f, %f, %f)", material.mEmissive.mR, material.mEmissive.mG, material.mEmissive.mB);
	if (GetLogging()) LOG("    + Shine         : %f", material.mShine);
	if (GetLogging()) LOG("    + Shine strength: %f", material.mShineStrength);
	if (GetLogging()) LOG("    + Opacity       : %f", material.mOpacity);
	if (GetLogging()) LOG("    + IOR           : %f", material.mIOR);
	if (GetLogging()) LOG("    + Double sided  : %d", material.mDoubleSided);
	if (GetLogging()) LOG("    + WireFrame     : %d", material.mWireFrame);
	if (GetLogging()) LOG("    + *** Material offset/tiling/rotation NOT available ***");


	// create the material
	MCore::Pointer<Material> stdMat( new StandardMaterial(material.mName) );
	StandardMaterial* mat = (StandardMaterial*)stdMat.GetPointer();
	
	// setup its properties
	mat->SetAmbient		( RGBAColor(material.mAmbient.mR, material.mAmbient.mG, material.mAmbient.mB) );
	mat->SetDiffuse		( RGBAColor(material.mDiffuse.mR, material.mDiffuse.mG, material.mDiffuse.mB) );
	mat->SetSpecular	( RGBAColor(material.mSpecular.mR, material.mSpecular.mG, material.mSpecular.mB) );
	mat->SetEmissive	( RGBAColor(material.mEmissive.mR, material.mEmissive.mG, material.mEmissive.mB) );
	mat->SetDoubleSided	( material.mDoubleSided );
	mat->SetIOR			( material.mIOR );
	mat->SetOpacity		( material.mOpacity );
	mat->SetShine		( material.mShine );
	mat->SetWireFrame	( material.mWireFrame );
	mat->SetShineStrength( material.mShineStrength );

	// add the material to the actor
	actor->AddMaterial(0,  stdMat );

	return true;
}


//=================================================================================================


// constructor
MaterialLayerChunkProcessor1::MaterialLayerChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MATERIALLAYER, 1)
{
}


// destructor
MaterialLayerChunkProcessor1::~MaterialLayerChunkProcessor1()
{
}


bool MaterialLayerChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");

		return false;
	}


	// read the layer from disk
	LMA_MaterialLayer matLayer;
	file->Read(&matLayer, sizeof(LMA_MaterialLayer));

	// convert the layer type
	int layerType;
	switch (matLayer.mMapType)
	{
		case LMA_LAYERID_DIFFUSE:		layerType = StandardMaterialLayer::LAYERTYPE_DIFFUSE;		break;
		case LMA_LAYERID_OPACITY:		layerType = StandardMaterialLayer::LAYERTYPE_OPACITY;		break;
		case LMA_LAYERID_SHINE:			layerType = StandardMaterialLayer::LAYERTYPE_SHINE;			break;
		case LMA_LAYERID_SHINESTRENGTH:	layerType = StandardMaterialLayer::LAYERTYPE_SHINESTRENGTH;	break;
		case LMA_LAYERID_SELFILLUM:		layerType = StandardMaterialLayer::LAYERTYPE_SELFILLUM;		break;
		case LMA_LAYERID_AMBIENT:		layerType = StandardMaterialLayer::LAYERTYPE_AMBIENT;		break;
		case LMA_LAYERID_SPECULAR:		layerType = StandardMaterialLayer::LAYERTYPE_SPECULAR;		break;
		case LMA_LAYERID_REFLECT:		layerType = StandardMaterialLayer::LAYERTYPE_REFLECT;		break;
		case LMA_LAYERID_REFRACT:		layerType = StandardMaterialLayer::LAYERTYPE_REFRACT;		break;
		case LMA_LAYERID_BUMP:			layerType = StandardMaterialLayer::LAYERTYPE_BUMP;			break;
		case LMA_LAYERID_FILTERCOLOR:	layerType = StandardMaterialLayer::LAYERTYPE_FILTERCOLOR;	break;
		case LMA_LAYERID_ENVIRONMENT:	layerType = StandardMaterialLayer::LAYERTYPE_ENVIRONMENT;	break;
		default:						layerType = StandardMaterialLayer::LAYERTYPE_UNKNOWN;
	};

	// create the layer
	StandardMaterialLayer* layer = new StandardMaterialLayer(layerType, matLayer.mTexture, matLayer.mAmount);

	// add the layer to the material
	Material* baseMat = actor->GetMaterial(0, matLayer.mMaterialNumber );
	assert( baseMat->GetType() == StandardMaterial::TYPE_ID );
	StandardMaterial* mat = (StandardMaterial*)baseMat;
	
	mat->AddLayer( layer );

	if (GetLogging()) LOG("    - Material Layer");
	if (GetLogging()) LOG("       + Texture  = '%s'", matLayer.mTexture);
	if (GetLogging()) LOG("       + Material = '%s' (%d)", mat->GetName().AsChar(), matLayer.mMaterialNumber);
	if (GetLogging()) LOG("       + Amount   = %f", matLayer.mAmount);
	if (GetLogging()) LOG("       + MapType  = %d", matLayer.mMapType);

	return true;
}


//=================================================================================================


// constructor
MotionPartChunkProcessor1::MotionPartChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MOTIONPART, 1)
{
}


// destructor
MotionPartChunkProcessor1::~MotionPartChunkProcessor1()
{
}


bool MotionPartChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!motion)
	{
		if (GetLogging())
			LOG_ERROR("Passed motion seems to be not valid!");

		return false;
	}

	if (GetLogging()) LOG("- Motion Part:");

	file->Read(&mMotionPart, sizeof(LMA_MotionPart));

	if (GetLogging()) LOG("    + Name = '%s'", mMotionPart.mName);
	if (GetLogging()) LOG("    + Pose Position: x=%f, y=%f, z=%f", mMotionPart.mPosePos.mX, mMotionPart.mPosePos.mY, mMotionPart.mPosePos.mZ);
	if (GetLogging()) LOG("    + Pose Rotation: x=%f, y=%f, z=%f, w=%f", mMotionPart.mPoseRot.mX, mMotionPart.mPoseRot.mY, mMotionPart.mPoseRot.mZ, mMotionPart.mPoseRot.mW);
	if (GetLogging()) LOG("    + Pose Scale:    x=%f, y=%f, z=%f", mMotionPart.mPoseScale.mX, mMotionPart.mPoseScale.mY, mMotionPart.mPoseScale.mZ);


	// create the part, and add it to the motion
	MotionPart* motionPart = new MotionPart(mMotionPart.mName);

	motionPart->SetPosePos	( Vector3(mMotionPart.mPosePos.mX,		mMotionPart.mPosePos.mY,	mMotionPart.mPosePos.mZ) );
	motionPart->SetPoseScale( Vector3(mMotionPart.mPoseScale.mX,	mMotionPart.mPoseScale.mY,	mMotionPart.mPoseScale.mZ) );
	motionPart->SetPoseRot	( Quaternion(mMotionPart.mPoseRot.mX,	mMotionPart.mPoseRot.mY,	mMotionPart.mPoseRot.mZ,	mMotionPart.mPoseRot.mW) );

	if (motion->GetType() == SkeletalMotion::TYPE_ID)
	{
		// cast
		SkeletalMotion* skelMotion = (SkeletalMotion*)motion;
		skelMotion->AddPart(motionPart);
	}

	// set last node
	Importer::SharedMotionInfo* sharedMotionInfo = (Importer::SharedMotionInfo*)mLMAImporter->FindSharedData( Importer::SharedMotionInfo::TYPE_ID );

	if (sharedMotionInfo)
		sharedMotionInfo->mLastPart = motionPart;
	else
	{
		delete motionPart;
		if (GetLogging())
			LOG("Can't find shared data.");

		return false;
	}

	return true;
}


//=================================================================================================


// constructor
AnimationChunkProcessor1::AnimationChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_ANIM, 1)
{
}


// destructor
AnimationChunkProcessor1::~AnimationChunkProcessor1()
{
}


bool AnimationChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!motion)
	{
		if (GetLogging())
			LOG("Passed motion seems to be not valid!");

		return false;
	}

	if (GetLogging()) LOG("    - Animation");

	file->Read(&mAnim, sizeof(LMA_Anim));

	if (GetLogging()) LOG("       + NrKeys    = %d", mAnim.mNrKeys);
	if (GetLogging()) LOG("       + AnimType  = %c", mAnim.mAnimType);
	if (GetLogging()) LOG("       + IPType    = %c", mAnim.mIPType);

	MotionPart* motionPart = NULL;

	// find lase motion part
	Importer::SharedMotionInfo* sharedMotionInfo = (Importer::SharedMotionInfo*)mLMAImporter->FindSharedData( Importer::SharedMotionInfo::TYPE_ID );

	if (sharedMotionInfo)
	{
		motionPart = sharedMotionInfo->mLastPart;
	}
	else
	{
		if (GetLogging())
			LOG("Can't find shared data.");

		return false;
	}

	// check if it is valid
	if (!motionPart)
	{
		if (GetLogging())
			LOG("Can't get last motion part.");

		return false;
	}


	// read the key data
	for (int i=0; i<mAnim.mNrKeys; i++)
	{
		if (mAnim.mAnimType == 'P')	// position
		{
			LMA_Vector3Key key;
			file->Read(&key, sizeof(LMA_Vector3Key));
			motionPart->GetPosTrack().AddKey( key.mTime, Vector3(key.mValue.mX, key.mValue.mY, key.mValue.mZ) );
			//if (GetLogging()) LOG("       + Translation keyframe: Nr=%i, x=%f, y=%f, z=%f", i, key.mValue.mX, key.mValue.mY, key.mValue.mZ);
		}
		else
		if (mAnim.mAnimType == 'R')	// rotation
		{
			LMA_QuaternionKey key;
			file->Read(&key, sizeof(LMA_QuaternionKey));
			motionPart->GetRotTrack().AddKey( key.mTime, Quaternion(key.mValue.mX, key.mValue.mY, key.mValue.mZ, key.mValue.mW)/*.LogN()*/ );
			//if (GetLogging()) LOG("       + Rotation keyframe: Nr=%i, x=%f, y=%f, z=%f, w=%f", i, key.mValue.mX, key.mValue.mY, key.mValue.mZ, key.mValue.mW);
		}
		else
		if (mAnim.mAnimType == 'S')	// scaling
		{
			LMA_Vector3Key key;
			file->Read(&key, sizeof(LMA_Vector3Key));
			motionPart->GetScaleTrack().AddKey( key.mTime, Vector3(key.mValue.mX, key.mValue.mY, key.mValue.mZ) );
			//if (GetLogging()) LOG("       + Scale keyframe: Nr=%i, x=%f, y=%f, z=%f", i, key.mValue.mX, key.mValue.mY, key.mValue.mZ);
		}
	}

	// init the track
	// position
	if (mAnim.mAnimType=='P')	
	{
//				motionPart->GetPosTrack().SetInterpolator( new HermiteInterpolator<Vector3>() );
//				motionPart->GetPosTrack().SetInterpolator( new BezierInterpolator<Vector3>() );
//				motionPart->GetPosTrack().SetInterpolator( new TCBSplineInterpolator<Vector3>() );
		motionPart->GetPosTrack().SetInterpolator( new LinearInterpolator<Vector3, Vector3>() );
		motionPart->GetPosTrack().Init();
	}

	// rotation
	if (mAnim.mAnimType=='R')
	{
//				motionPart->GetRotTrack().SetInterpolator( new HermiteQuaternionInterpolator() );
//				motionPart->GetRotTrack().SetInterpolator( new TCBSplineQuaternionInterpolator() );
		motionPart->GetRotTrack().SetInterpolator( new LinearQuaternionInterpolator<Compressed16BitQuaternion>() );
		motionPart->GetRotTrack().Init();
	}

	// scaling
	if (mAnim.mAnimType=='S')
	{
		motionPart->GetScaleTrack().SetInterpolator( new LinearInterpolator<Vector3, Vector3>() );
		motionPart->GetScaleTrack().Init();		
	}

	return true;
}


//=================================================================================================


// constructor
MaterialChunkProcessor2::MaterialChunkProcessor2(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MATERIAL, 2)
{
}
 

// destructor
MaterialChunkProcessor2::~MaterialChunkProcessor2()
{
}


bool MaterialChunkProcessor2::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");

		return false;
	}

	LMA_Material2 material;
	file->Read(&material, sizeof(LMA_Material2));

	// print material information
	if (GetLogging()) LOG("- Material name = '%s'", material.mName);
	if (GetLogging()) LOG("    + Ambient       : (%f, %f, %f)", material.mAmbient.mR, material.mAmbient.mG, material.mAmbient.mB);
	if (GetLogging()) LOG("    + Diffuse       : (%f, %f, %f)", material.mDiffuse.mR, material.mDiffuse.mG, material.mDiffuse.mB);
	if (GetLogging()) LOG("    + Specular      : (%f, %f, %f)", material.mSpecular.mR, material.mSpecular.mG, material.mSpecular.mB);
	if (GetLogging()) LOG("    + Emissive      : (%f, %f, %f)", material.mEmissive.mR, material.mEmissive.mG, material.mEmissive.mB);
	if (GetLogging()) LOG("    + Shine         : %f", material.mShine);
	if (GetLogging()) LOG("    + Shine strength: %f", material.mShineStrength);
	if (GetLogging()) LOG("    + Opacity       : %f", material.mOpacity);
	if (GetLogging()) LOG("    + IOR           : %f", material.mIOR);
	if (GetLogging()) LOG("    + Double sided  : %d", material.mDoubleSided);
	if (GetLogging()) LOG("    + WireFrame     : %d", material.mWireFrame);

	// create the material
	MCore::Pointer<Material> stdMat( new StandardMaterial(material.mName) );
	StandardMaterial* mat = (StandardMaterial*)stdMat.GetPointer();

	// setup its properties
	mat->SetAmbient		( RGBAColor(material.mAmbient.mR, material.mAmbient.mG, material.mAmbient.mB) );
	mat->SetDiffuse		( RGBAColor(material.mDiffuse.mR, material.mDiffuse.mG, material.mDiffuse.mB) );
	mat->SetSpecular	( RGBAColor(material.mSpecular.mR, material.mSpecular.mG, material.mSpecular.mB) );
	mat->SetEmissive	( RGBAColor(material.mEmissive.mR, material.mEmissive.mG, material.mEmissive.mB) );
	mat->SetDoubleSided	( material.mDoubleSided );
	mat->SetIOR			( material.mIOR );
	mat->SetOpacity		( material.mOpacity );
	mat->SetShine		( material.mShine );
	mat->SetWireFrame	( material.mWireFrame );
	mat->SetShineStrength( material.mShineStrength );

	mat->mShaderMask = 0;
	mat->mAlphaRef   = 127;
	mat->mSrcBlend = 4;
	mat->mDstBlend = 5;

	// add the material to the actor
	actor->AddMaterial(0,  stdMat);

	return true;
}


//=================================================================================================


// constructor
MaterialChunkProcessor3::MaterialChunkProcessor3(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MATERIAL, 3)
{
}


// destructor
MaterialChunkProcessor3::~MaterialChunkProcessor3()
{
}


bool MaterialChunkProcessor3::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");

		return false;
	}

	LMA_Material3 material;
	file->Read(&material, sizeof(LMA_Material3));

	// print material information
	if (GetLogging()) LOG("- Material name = '%s'", material.mName);
	if (GetLogging()) LOG("    + Ambient       : (%f, %f, %f)", material.mAmbient.mR, material.mAmbient.mG, material.mAmbient.mB);
	if (GetLogging()) LOG("    + Diffuse       : (%f, %f, %f)", material.mDiffuse.mR, material.mDiffuse.mG, material.mDiffuse.mB);
	if (GetLogging()) LOG("    + Specular      : (%f, %f, %f)", material.mSpecular.mR, material.mSpecular.mG, material.mSpecular.mB);
	if (GetLogging()) LOG("    + Emissive      : (%f, %f, %f)", material.mEmissive.mR, material.mEmissive.mG, material.mEmissive.mB);
	if (GetLogging()) LOG("    + Shine         : %f", material.mShine);
	if (GetLogging()) LOG("    + Shine strength: %f", material.mShineStrength);
	if (GetLogging()) LOG("    + Opacity       : %f", material.mOpacity);
	if (GetLogging()) LOG("    + IOR           : %f", material.mIOR);
	if (GetLogging()) LOG("    + Double sided  : %d", material.mDoubleSided);
	if (GetLogging()) LOG("    + WireFrame     : %d", material.mWireFrame);

	// create the material
	MCore::Pointer<Material> stdMat( new StandardMaterial(material.mName) );
	StandardMaterial* mat = (StandardMaterial*)stdMat.GetPointer();
	
	// setup its properties
	mat->SetAmbient		( RGBAColor(material.mAmbient.mR, material.mAmbient.mG, material.mAmbient.mB) );
	mat->SetDiffuse		( RGBAColor(material.mDiffuse.mR, material.mDiffuse.mG, material.mDiffuse.mB) );
	mat->SetSpecular	( RGBAColor(material.mSpecular.mR, material.mSpecular.mG, material.mSpecular.mB) );
	mat->SetEmissive	( RGBAColor(material.mEmissive.mR, material.mEmissive.mG, material.mEmissive.mB) );
	mat->SetDoubleSided	( material.mDoubleSided );
	mat->SetIOR			( material.mIOR );
	mat->SetOpacity		( material.mOpacity );
	mat->SetShine		( material.mShine );
	mat->SetWireFrame	( material.mWireFrame );
	mat->SetShineStrength( material.mShineStrength );

	// add the material to the actor
	actor->AddMaterial(0,  stdMat);

	return true;
}


//=================================================================================================


// constructor
MaterialChunkProcessor10000::MaterialChunkProcessor10000(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MATERIAL, 10000)
{
}


// destructor
MaterialChunkProcessor10000::~MaterialChunkProcessor10000()
{
}


bool MaterialChunkProcessor10000::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)
		
		if (!actor)
		{
			if (GetLogging())
				LOG("Passed actor seems to be not valid!");
			
			return false;
		}
		
		LMA_Material10000 material;
		file->Read(&material, sizeof(LMA_Material10000));
		
		// print material information
		if (GetLogging()) LOG("- Material name = '%s'", material.mName);
		if (GetLogging()) LOG("    + Ambient: r=%f g=%f b=%f", material.mAmbient.mR, material.mAmbient.mG, material.mAmbient.mB);
		if (GetLogging()) LOG("    + Diffuse: r=%f g=%f b=%f", material.mDiffuse.mR, material.mDiffuse.mG, material.mDiffuse.mB);
		if (GetLogging()) LOG("    + Specular: r=%f g=%f b=%f", material.mSpecular.mR, material.mSpecular.mG, material.mSpecular.mB);
		if (GetLogging()) LOG("    + Emissive: r=%f g=%f b=%f", material.mEmissive.mR, material.mEmissive.mG, material.mEmissive.mB);
		if (GetLogging()) LOG("    + Shine: %f", material.mShine);
		if (GetLogging()) LOG("    + ShineStrength: %f", material.mShineStrength);
		if (GetLogging()) LOG("    + Opacity: %f", material.mOpacity);
		if (GetLogging()) LOG("    + IndexOfRefraction: %f", material.mIOR);
		if (GetLogging()) LOG("    + DoubleSided: %d", material.mDoubleSided);
		if (GetLogging()) LOG("    + WireFrame: %d", material.mWireFrame);
		if (GetLogging()) LOG("    + TransparencyType: %c", material.mTransparencyType);
		
		// create the material
//		StandardMaterial * mat = new StandardMaterial(material.mName);
//		MCore::Pointer<Material> lmMat(mat);

		MCore::Pointer<Material> stdMat( new StandardMaterial(material.mName) );
		StandardMaterial* mat = (StandardMaterial*)stdMat.GetPointer();
		
		// setup its properties
		mat->SetAmbient		( RGBAColor(material.mAmbient.mR, material.mAmbient.mG, material.mAmbient.mB) );
		mat->SetDiffuse		( RGBAColor(material.mDiffuse.mR, material.mDiffuse.mG, material.mDiffuse.mB) );
		mat->SetSpecular	( RGBAColor(material.mSpecular.mR, material.mSpecular.mG, material.mSpecular.mB) );
		mat->SetEmissive	( RGBAColor(material.mEmissive.mR, material.mEmissive.mG, material.mEmissive.mB) );
		mat->SetDoubleSided	( material.mDoubleSided );
		mat->SetIOR			( material.mIOR );
		mat->SetOpacity		( material.mOpacity );
		mat->SetShine		( material.mShine );
		mat->SetWireFrame	( material.mWireFrame );
		mat->SetShineStrength	( material.mShineStrength );
		//mat->SetTransparencyType( material.mTransparencyType );	// TODO: what to do? :)

		mat->mShaderMask = material.mShaderMask;
		mat->mAlphaRef   = material.mAlphaRef;
		mat->mSrcBlend   = material.mSrcBlend;
		mat->mDstBlend   = material.mDstBlend; 
		mat->mTexFxn     = material.mTexFxn;
		mat->mTexFxnUParm = material.mTexFxnUParm0;
		mat->mTexFxnVParm = material.mTexFxnVParm0;
		mat->mTexFxnSub0Parm = material.mTexFxnSub0Parm0;

		// add the material to the actor
		actor->AddMaterial(0,  stdMat );

		return true;
}

//=================================================================================================


// constructor
MaterialLayerChunkProcessor2::MaterialLayerChunkProcessor2(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MATERIALLAYER, 2)
{
}


// destructor
MaterialLayerChunkProcessor2::~MaterialLayerChunkProcessor2()
{
}


bool MaterialLayerChunkProcessor2::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");

		return false;
	}

	// read the layer from disk
	LMA_MaterialLayer2 matLayer;
	file->Read(&matLayer, sizeof(LMA_MaterialLayer2));

	// convert the layer type
	int layerType;
	switch (matLayer.mMapType)
	{
		case LMA_LAYERID_DIFFUSE:		layerType = StandardMaterialLayer::LAYERTYPE_DIFFUSE;			break;
		case LMA_LAYERID_OPACITY:		layerType = StandardMaterialLayer::LAYERTYPE_OPACITY;			break;
		case LMA_LAYERID_SHINE:			layerType = StandardMaterialLayer::LAYERTYPE_SHINE;				break;
		case LMA_LAYERID_SHINESTRENGTH:	layerType = StandardMaterialLayer::LAYERTYPE_SHINESTRENGTH;		break;
		case LMA_LAYERID_SELFILLUM:		layerType = StandardMaterialLayer::LAYERTYPE_SELFILLUM;			break;
		case LMA_LAYERID_AMBIENT:		layerType = StandardMaterialLayer::LAYERTYPE_AMBIENT;			break;
		case LMA_LAYERID_SPECULAR:		layerType = StandardMaterialLayer::LAYERTYPE_SPECULAR;			break;
		case LMA_LAYERID_REFLECT:		layerType = StandardMaterialLayer::LAYERTYPE_REFLECT;			break;
		case LMA_LAYERID_REFRACT:		layerType = StandardMaterialLayer::LAYERTYPE_REFRACT;			break;
		case LMA_LAYERID_BUMP:			layerType = StandardMaterialLayer::LAYERTYPE_BUMP;				break;
		case LMA_LAYERID_FILTERCOLOR:	layerType = StandardMaterialLayer::LAYERTYPE_FILTERCOLOR;		break;
		case LMA_LAYERID_ENVIRONMENT:	layerType = StandardMaterialLayer::LAYERTYPE_ENVIRONMENT;		break;
		default:						layerType = StandardMaterialLayer::LAYERTYPE_UNKNOWN;
	};

	// create the layer
	StandardMaterialLayer* layer = new StandardMaterialLayer(layerType, matLayer.mTexture, matLayer.mAmount);

	// add the layer to the material
	Material* baseMat = actor->GetMaterial(0, matLayer.mMaterialNumber ).GetPointer();
	assert(baseMat->GetType() == StandardMaterial::TYPE_ID);
	StandardMaterial* mat = (StandardMaterial*)baseMat;
	mat->AddLayer( layer );

	// set the properties
	layer->SetRotationRadians( matLayer.mRotationRadians );
	layer->SetUOffset( matLayer.mUOffset );
	layer->SetVOffset( matLayer.mVOffset );
	layer->SetUTiling( matLayer.mUTiling );
	layer->SetVTiling( matLayer.mVTiling );

	if (GetLogging()) LOG("    - Material Layer");
	if (GetLogging()) LOG("       + Texture  = %s", matLayer.mTexture);
	if (GetLogging()) LOG("       + Material = %s (number %d)", mat->GetName().GetReadPtr(), matLayer.mMaterialNumber);
	if (GetLogging()) LOG("       + Amount   = %f", matLayer.mAmount);
	if (GetLogging()) LOG("       + MapType  = %d", matLayer.mMapType);
	if (GetLogging()) LOG("       + UOffset  = %f", matLayer.mUOffset);
	if (GetLogging()) LOG("       + VOffset  = %f", matLayer.mVOffset);
	if (GetLogging()) LOG("       + UTiling  = %f", matLayer.mUTiling);
	if (GetLogging()) LOG("       + VTiling  = %f", matLayer.mVTiling);
	if (GetLogging()) LOG("       + Rotation = %f (radians)", matLayer.mRotationRadians);

	return true;
}


//=================================================================================================


// constructor
LimitChunkProcessor1::LimitChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_LIMIT, 1)
{
}


// destructor
LimitChunkProcessor1::~LimitChunkProcessor1()
{
}


bool LimitChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");

		return false;
	}

	// read the limit from disk
	LMA_Limit lmaLimit;
	file->Read(&lmaLimit, sizeof(LMA_Limit));

	// create our limit attribute
	NodeLimitAttribute* limit = new NodeLimitAttribute();

	// set translation minimums and maximums
	limit->SetTranslationMin(Vector3(lmaLimit.mTranslationMin.mX, lmaLimit.mTranslationMin.mY, lmaLimit.mTranslationMin.mZ));
	limit->SetTranslationMax(Vector3(lmaLimit.mTranslationMax.mX, lmaLimit.mTranslationMax.mY, lmaLimit.mTranslationMax.mZ));

	// set rotation minimums and maximums
	limit->SetRotationMin(Vector3(lmaLimit.mRotationMin.mX, lmaLimit.mRotationMin.mY, lmaLimit.mRotationMin.mZ));
	limit->SetRotationMax(Vector3(lmaLimit.mRotationMax.mX, lmaLimit.mRotationMax.mY, lmaLimit.mRotationMax.mZ));

	// set scale minimums and maximums
	limit->SetScaleMin(Vector3(lmaLimit.mScaleMin.mX, lmaLimit.mScaleMin.mY, lmaLimit.mScaleMin.mZ));
	limit->SetScaleMax(Vector3(lmaLimit.mScaleMax.mX, lmaLimit.mScaleMax.mY, lmaLimit.mScaleMax.mZ));

	// set activation flags
	limit->EnableLimit(NodeLimitAttribute::TRANSLATIONX,	lmaLimit.mLimitFlags[0]);
	limit->EnableLimit(NodeLimitAttribute::TRANSLATIONY,	lmaLimit.mLimitFlags[1]);
	limit->EnableLimit(NodeLimitAttribute::TRANSLATIONZ,	lmaLimit.mLimitFlags[2]);
	limit->EnableLimit(NodeLimitAttribute::ROTATIONX,	lmaLimit.mLimitFlags[3]);
	limit->EnableLimit(NodeLimitAttribute::ROTATIONY,	lmaLimit.mLimitFlags[4]);
	limit->EnableLimit(NodeLimitAttribute::ROTATIONZ,	lmaLimit.mLimitFlags[5]);
	limit->EnableLimit(NodeLimitAttribute::SCALEX,		lmaLimit.mLimitFlags[6]);
	limit->EnableLimit(NodeLimitAttribute::SCALEY,		lmaLimit.mLimitFlags[7]);
	limit->EnableLimit(NodeLimitAttribute::SCALEZ,		lmaLimit.mLimitFlags[8]);

	// try to get the node to which the limit belongs to
	Node* node = actor->GetNode( lmaLimit.mNodeNumber );

	// add the limit attribute to the node
	node->AddAttribute(limit);

	// print limit information
	if (GetLogging()) LOG("- Limits for node '%s'", node->GetName().AsChar());
	if (GetLogging()) LOG("    + TranslateMin = %.3f, %.3f, %.3f", lmaLimit.mTranslationMin.mX, lmaLimit.mTranslationMin.mY, lmaLimit.mTranslationMin.mZ);
	if (GetLogging()) LOG("    + TranslateMax = %.3f, %.3f, %.3f", lmaLimit.mTranslationMax.mX, lmaLimit.mTranslationMax.mY, lmaLimit.mTranslationMax.mZ);
	if (GetLogging()) LOG("    + RotateMin    = %.3f, %.3f, %.3f", lmaLimit.mRotationMin.mX, lmaLimit.mRotationMin.mY, lmaLimit.mRotationMin.mZ);
	if (GetLogging()) LOG("    + RotateMax    = %.3f, %.3f, %.3f", lmaLimit.mRotationMax.mX, lmaLimit.mRotationMax.mY, lmaLimit.mRotationMax.mZ);
	if (GetLogging()) LOG("    + ScaleMin     = %.3f, %.3f, %.3f", lmaLimit.mScaleMin.mX, lmaLimit.mScaleMin.mY, lmaLimit.mScaleMin.mZ);
	if (GetLogging()) LOG("    + ScaleMax     = %.3f, %.3f, %.3f", lmaLimit.mScaleMax.mX, lmaLimit.mScaleMax.mY, lmaLimit.mScaleMax.mZ);
	if (GetLogging()) LOG("    + LimitFlags   = POS[%d, %d, %d], ROT[%d, %d, %d], SCALE[%d, %d, %d]", lmaLimit.mLimitFlags[0], lmaLimit.mLimitFlags[1], lmaLimit.mLimitFlags[2], lmaLimit.mLimitFlags[3], lmaLimit.mLimitFlags[4], lmaLimit.mLimitFlags[5], lmaLimit.mLimitFlags[6], lmaLimit.mLimitFlags[7], lmaLimit.mLimitFlags[8]);

	return true;
}


//=================================================================================================


// constructor
PhysicsInfoChunkProcessor1::PhysicsInfoChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_PHYSICSINFO, 1)
{
}


// destructor
PhysicsInfoChunkProcessor1::~PhysicsInfoChunkProcessor1()
{
}


bool PhysicsInfoChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");

		return false;
	}

	// read the physics info from disk
	LMA_PhysicsInfo lmaPhysicsInfo;
	file->Read(&lmaPhysicsInfo, sizeof(LMA_PhysicsInfo));

	// create our physics info attribute
	NodePhysicsAttribute* physicsAttribute = new NodePhysicsAttribute();

	// set the mass
	physicsAttribute->SetMass( lmaPhysicsInfo.mMass );

	// create the physics object
	switch (lmaPhysicsInfo.mPhysicsObjectType)
	{
		// box
		case 0:
		{
			// create our box and set the width, height and depth
			NodePhysicsAttribute::Box* box = new NodePhysicsAttribute::Box;
			box->SetWidth(lmaPhysicsInfo.mBoxWidth);
			box->SetHeight(lmaPhysicsInfo.mBoxHeight);
			box->SetDepth(lmaPhysicsInfo.mBoxDepth);
			physicsAttribute->SetPhysicsObject(box);
			break;
		}

		// sphere
		case 1:
		{
			// create our sphere and set the radius
			NodePhysicsAttribute::Sphere* sphere = new NodePhysicsAttribute::Sphere;
			sphere->SetRadius(lmaPhysicsInfo.mSphereRadius);
			physicsAttribute->SetPhysicsObject(sphere);
			break;
		}

		// cylinder
		case 2:
		{
			// create our cylinder and set height and radius
			NodePhysicsAttribute::Cylinder* cylinder = new NodePhysicsAttribute::Cylinder;
			cylinder->SetRadius(lmaPhysicsInfo.mCylinderRadius);
			cylinder->SetHeight(lmaPhysicsInfo.mCylinderHeight);
			physicsAttribute->SetPhysicsObject(cylinder);
			break;
		}

		default:
			if (GetLogging()) LOG("    + Physics object type = %d (unsupported)", lmaPhysicsInfo.mPhysicsObjectType);
	}

	// try to get the node to which the physics info belongs to
	Node* node = actor->GetNodeByName(lmaPhysicsInfo.mName);

	if (node)
	{
		// add the physics info attribute to the node
		node->AddAttribute(physicsAttribute);
	}
	else
	{
		String errorMsg;
		errorMsg.Format("Error getting node %s.", lmaPhysicsInfo.mName);
		if (GetLogging()) LOG_ERROR(errorMsg.AsChar());

		// we haven't found the node to which the physics info belongs to
		// get rid of the physics attribute
		delete physicsAttribute;
	}

	return true;
}


//=================================================================================================


// constructor
MeshExpressionPartChunkProcessor1::MeshExpressionPartChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MESHEXPRESSIONPART, 1)
{
}


// destructor
MeshExpressionPartChunkProcessor1::~MeshExpressionPartChunkProcessor1()
{
}


bool MeshExpressionPartChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)
	if (GetLogging()) LOG("- Reading mesh expression part");

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be invalid!");

		return false;
	}

	// read the expression part from disk
	LMA_MeshExpressionPart lmaExpressionPart;
	file->Read(&lmaExpressionPart, sizeof(LMA_MeshExpressionPart));
	if (GetLogging()) LOG("    + Name          = %s", lmaExpressionPart.mName);
	if (GetLogging()) LOG("    + LOD Level     = %d", lmaExpressionPart.mLOD);
	if (GetLogging()) LOG("    + RangeMin      = %d", lmaExpressionPart.mRangeMin);
	if (GetLogging()) LOG("    + RangeMax      = %d", lmaExpressionPart.mRangeMax);
	if (GetLogging()) LOG("    + NumDeforms    = %d", lmaExpressionPart.mNumDeformDatas);
	if (GetLogging()) LOG("    + NumTransforms = %d", lmaExpressionPart.mNumTransformations);
	if (GetLogging()) LOG("    + IsPhoneme     = %s", (lmaExpressionPart.mPhonemeChar==0) ? "No" : "Yes");
	if (GetLogging()) LOG("    + Phoneme Sets  = %d", lmaExpressionPart.mNumPhonemeSets);

	// get the level of detail of the expression part
	int expressionPartLOD = lmaExpressionPart.mLOD;

	// get the char to which this expression(phoneme) belongs to
	char phonemeChar = lmaExpressionPart.mPhonemeChar;

	// check if the facial setup has already been created, if not create it
	if (actor->GetFacialSetup(expressionPartLOD).GetPointer() == NULL)
	{
		// create the facial setup
		Pointer<FacialSetup> facialSetup(new FacialSetup());

		// bind it
		actor->SetFacialSetup(expressionPartLOD, facialSetup);
	}

	// get the expression name
	String expressionName = lmaExpressionPart.mName;

	// create the mesh expression part
	MeshExpressionPart* expressionPart = new MeshExpressionPart(true, true, actor, expressionName);

	expressionPart->SetPhonemeCharacter(lmaExpressionPart.mPhonemeChar);
	//expressionPart->SetPhonemeSet((BonesExpressionPart::EPhonemeSet)lmaExpressionPart.mPhonemeSet);

	if (phonemeChar == 0)
	{
		// add the expression part to the facial setup
		actor->GetFacialSetup(expressionPartLOD)->AddExpressionPart(expressionPart);
	}
	else
	{
		// add the phoneme to the facial setup
		actor->GetFacialSetup(expressionPartLOD)->AddPhoneme(expressionPart);
	}

	// set the slider range
	expressionPart->SetRangeMin(lmaExpressionPart.mRangeMin);
	expressionPart->SetRangeMax(lmaExpressionPart.mRangeMax);

	// read all deform datas
	if (GetLogging()) LOG("    - Reading deform datas...");
	int i;
	for (i=0; i<lmaExpressionPart.mNumDeformDatas; i++)
	{
		LMA_FacialDeformData lmaDeformData;
		file->Read(&lmaDeformData, sizeof(LMA_FacialDeformData));

		if (GetLogging()) LOG("    + Deform data info:");
		if (GetLogging()) LOG("       - Node               = %s", lmaDeformData.mNodeName );
		if (GetLogging()) LOG("       - NumVertices        = %d", lmaDeformData.mNumVertices);

		Node* deformNode = actor->GetNodeByName( lmaDeformData.mNodeName );
		assert(deformNode != NULL);

		MeshExpressionPart::DeformData* deformData = new MeshExpressionPart::DeformData(deformNode, lmaDeformData.mNumVertices);
		expressionPart->AddDeformData( deformData );

		//-------------------------------
		// create the mesh deformer
		MCore::Pointer<MeshDeformerStack>& stack = deformNode->GetMeshDeformerStack(0);

		// create the stack if it doesn't yet exist
		if (stack.GetPointer() == NULL)
		{
			stack = MCore::Pointer<MeshDeformerStack>( new MeshDeformerStack( deformNode->GetMesh(0) ) );
			deformNode->SetMeshDeformerStack(stack, 0);
		}

		// add the skinning deformer to the stack
		SmartMeshMorphDeformer* deformer = new SmartMeshMorphDeformer( expressionPart, i, deformNode->GetMesh(0) );
		stack->InsertDeformer(0, deformer);
		//-------------------------------

		// read the deltas
		if (GetLogging()) LOG("   + Reading deltas");
		const int numVerts = lmaDeformData.mNumVertices;

		// allocate
		LMA_Vector3* deltas = (LMA_Vector3*)MEMMGR.Allocate(numVerts * sizeof(LMA_Vector3), MEMCATEGORY_IMPORTER, 16, 666);	// blocktype 666 to make sure it doesn't trash any other blocks :)

		// read the deltas
		file->Read(deltas, numVerts * sizeof(LMA_Vector3));

		// calculate the bounding box around the deltas
		AABB box;
		int a;
		for (a=0; a<numVerts; a++)
			box.Encapsulate( Vector3(deltas[a].mX, deltas[a].mY, deltas[a].mZ) );

		// calculate the minimum value of the box
		float minValue = box.GetMin().x;
		minValue = MCore::Min<float>(minValue, box.GetMin().y);
		minValue = MCore::Min<float>(minValue, box.GetMin().z);

		// calculate the minimum value of the box
		float maxValue = box.GetMax().x;
		maxValue = MCore::Max<float>(maxValue, box.GetMax().y);
		maxValue = MCore::Max<float>(maxValue, box.GetMax().z);

		// make sure the values won't be too small and wil lead to compression errors
		if (maxValue - minValue < 1.0f)
		{
			if (minValue < 0 && minValue > -1)
				minValue = -1;

			if (maxValue > 0 && maxValue < 1)
				maxValue = 1;	
		}

		// update the deform data min and max
		deformData->mMinValue = minValue;
		deformData->mMaxValue = maxValue;

		// now set and compress the vectors
		for (a=0; a<numVerts; a++)
			deformData->mDeltas[a].FromVector3(Vector3(deltas[a].mX, deltas[a].mY, deltas[a].mZ), minValue, maxValue);

		// delete the temparory buffer
		MEMMGR.Delete( deltas );


		// read the normals
		LMA_Vector3 delta;
		for (a=0; a<numVerts; a++)
		{
			file->Read(&delta, sizeof(LMA_Vector3));
			deformData->mDeltaNormals[a].FromVector3( Vector3(delta.mX, delta.mY, delta.mZ), -1.0f, 1.0f);
		}

		// read the vertex numbers
		if (GetLogging()) LOG("   + Reading vertex numbers");
		int vtxNr;
		for (a=0; a<numVerts; a++)
		{
			file->Read(&vtxNr, sizeof(int));
			deformData->mVertexNumbers[a] = vtxNr;
		}
	}


	// read the facial transformations
	if (GetLogging()) LOG("    - Reading transformations...");
	for (i=0; i<lmaExpressionPart.mNumTransformations; i++)
	{
		// read the facial transformation from disk
		LMA_FacialTransformation lmaFacialTransformation;
		file->Read(&lmaFacialTransformation, sizeof(LMA_FacialTransformation));

		// try to get node id, if the given node doesn't exist skip the transformation
		String nodeName = lmaFacialTransformation.mNodeName;
		Node* node = actor->GetNodeByName(nodeName);
		if (!node)
		{
			LOG("Could not get node: %s", nodeName.AsChar());
			continue;
		}

		// create our transformation
		MeshExpressionPart::Transformation transform;
		transform.mPosition = Vector3(lmaFacialTransformation.mPosition.mX, lmaFacialTransformation.mPosition.mY, lmaFacialTransformation.mPosition.mZ);
		transform.mScale	= Vector3(lmaFacialTransformation.mScale.mX, lmaFacialTransformation.mScale.mY, lmaFacialTransformation.mScale.mZ);
		transform.mRotation	= Quaternion(lmaFacialTransformation.mRotation.mX, lmaFacialTransformation.mRotation.mY, lmaFacialTransformation.mRotation.mZ, lmaFacialTransformation.mRotation.mW);

		// add the transformation to the bones expression part
		expressionPart->AddTransformation(node->GetID(), transform);
	}

	// read the phoneme sets
	if (GetLogging()) LOG("    - Reading phoneme sets...");
	MCore::Array<ExpressionPart::EPhonemeSet> phonemeSets;
	phonemeSets.Resize( lmaExpressionPart.mNumPhonemeSets );
	for (i=0; i<lmaExpressionPart.mNumPhonemeSets; i++)
	{
		int set;
		file->Read(&set, sizeof(int));
		phonemeSets[i] = (ExpressionPart::EPhonemeSet)set;
	}

	// set the read phoneme sets
	expressionPart->SetPhonemeSets( phonemeSets );

	return true;
}


//=================================================================================================


// constructor
MeshExpressionPartChunkProcessor2::MeshExpressionPartChunkProcessor2(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_MESHEXPRESSIONPART, 2)
{
}


// destructor
MeshExpressionPartChunkProcessor2::~MeshExpressionPartChunkProcessor2()
{
}


bool MeshExpressionPartChunkProcessor2::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)
	if (GetLogging()) LOG("- Reading mesh expression part [v2]");

	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be invalid!");

		return false;
	}

	// read the expression part from disk
	LMA_MeshExpressionPart2 lmaExpressionPart;
	file->Read(&lmaExpressionPart, sizeof(LMA_MeshExpressionPart2));
	if (GetLogging()) LOG("    + Name          = %s", lmaExpressionPart.mName);
	if (GetLogging()) LOG("    + LOD Level     = %d", lmaExpressionPart.mLOD);
	if (GetLogging()) LOG("    + RangeMin      = %d", lmaExpressionPart.mRangeMin);
	if (GetLogging()) LOG("    + RangeMax      = %d", lmaExpressionPart.mRangeMax);
	if (GetLogging()) LOG("    + NumDeforms    = %d", lmaExpressionPart.mNumDeformDatas);
	if (GetLogging()) LOG("    + NumTransforms = %d", lmaExpressionPart.mNumTransformations);
	if (GetLogging()) LOG("    + IsPhoneme     = %s", (lmaExpressionPart.mPhonemeChar==0) ? "No" : "Yes");
	if (GetLogging()) LOG("    + Phoneme Sets  = %d", lmaExpressionPart.mNumPhonemeSets);

	// get the level of detail of the expression part
	int expressionPartLOD = lmaExpressionPart.mLOD;

	// get the char to which this expression(phoneme) belongs to
	char phonemeChar = lmaExpressionPart.mPhonemeChar;

	// check if the facial setup has already been created, if not create it
	if (actor->GetFacialSetup(expressionPartLOD).GetPointer() == NULL)
	{
		// create the facial setup
		Pointer<FacialSetup> facialSetup(new FacialSetup());

		// bind it
		actor->SetFacialSetup(expressionPartLOD, facialSetup);
	}

	// get the expression name
	String expressionName = lmaExpressionPart.mName;

	// create the mesh expression part
	MeshExpressionPart* expressionPart = new MeshExpressionPart(true, true, actor, expressionName);

	expressionPart->SetPhonemeCharacter(lmaExpressionPart.mPhonemeChar);
	//expressionPart->SetPhonemeSet((BonesExpressionPart::EPhonemeSet)lmaExpressionPart.mPhonemeSet);

	if (phonemeChar == 0)
	{
		// add the expression part to the facial setup
		actor->GetFacialSetup(expressionPartLOD)->AddExpressionPart(expressionPart);
	}
	else
	{
		// add the phoneme to the facial setup
		actor->GetFacialSetup(expressionPartLOD)->AddPhoneme(expressionPart);
	}

	// set the slider range
	expressionPart->SetRangeMin(lmaExpressionPart.mRangeMin);
	expressionPart->SetRangeMax(lmaExpressionPart.mRangeMax);

	// read all deform datas
	if (GetLogging()) LOG("    - Reading deform datas...");
	for (int i=0; i<lmaExpressionPart.mNumDeformDatas; i++)
	{
		LMA_FacialDeformData2 lmaDeformData;
		file->Read(&lmaDeformData, sizeof(LMA_FacialDeformData2));

		if (GetLogging()) LOG("    + Deform data info:");
		if (GetLogging()) LOG("       - Node        = %s", lmaDeformData.mNodeName );
		if (GetLogging()) LOG("       - NumVertices = %d", lmaDeformData.mNumVertices);

		Node* deformNode = actor->GetNodeByName( lmaDeformData.mNodeName );
		assert(deformNode != NULL);

		MeshExpressionPart::DeformData* deformData = new MeshExpressionPart::DeformData(deformNode, lmaDeformData.mNumVertices);
		expressionPart->AddDeformData( deformData );

		// set the min and max values, used to define the compression/quantitization range for the positions
		deformData->mMinValue = lmaDeformData.mMinValue;
		deformData->mMaxValue = lmaDeformData.mMaxValue;

		//-------------------------------
		// create the mesh deformer
		MCore::Pointer<MeshDeformerStack>& stack = deformNode->GetMeshDeformerStack(0);

		// create the stack if it doesn't yet exist
		if (stack.GetPointer() == NULL)
		{
			stack = MCore::Pointer<MeshDeformerStack>( new MeshDeformerStack( deformNode->GetMesh(0) ) );
			deformNode->SetMeshDeformerStack(stack, 0);
		}

		// add the skinning deformer to the stack
		SmartMeshMorphDeformer* deformer = new SmartMeshMorphDeformer( expressionPart, i, deformNode->GetMesh(0) );
		stack->InsertDeformer(0, deformer);
		//-------------------------------

		// read the deltas
		if (GetLogging()) LOG("   + Reading deltas");
		const int numVerts = lmaDeformData.mNumVertices;

		// read the positions
		LMA_16BitVector3 deltaPos;
		int d;
		for (d=0; d<numVerts; d++)
		{
			file->Read(&deltaPos, sizeof(LMA_16BitVector3));
			deformData->mDeltas[d] = Compressed16BitVector3(deltaPos.mX, deltaPos.mY, deltaPos.mZ);
		}

		// read the normals
		LMA_8BitVector3 delta;
		for (d=0; d<numVerts; d++)
		{
			file->Read(&delta, sizeof(LMA_8BitVector3));
			deformData->mDeltaNormals[d] = Compressed8BitVector3(delta.mX, delta.mY, delta.mZ);
		}

		// read the vertex numbers
		if (GetLogging()) LOG("   + Reading vertex numbers");
		int vtxNr;
		for (int a=0; a<numVerts; a++)
		{
			file->Read(&vtxNr, sizeof(int));
			deformData->mVertexNumbers[a] = vtxNr;
		}
	}


	// read the facial transformations
	if (GetLogging()) LOG("    - Reading transformations...");
	for (int i=0; i<lmaExpressionPart.mNumTransformations; i++)
	{
		// read the facial transformation from disk
		LMA_FacialTransformation lmaFacialTransformation;
		file->Read(&lmaFacialTransformation, sizeof(LMA_FacialTransformation));

		// try to get node id, if the given node doesn't exist skip the transformation
		String nodeName = lmaFacialTransformation.mNodeName;
		Node* node = actor->GetNodeByName(nodeName);
		if (!node)
		{
			LOG("Could not get node: %s", nodeName.AsChar());
			continue;
		}

		// create our transformation
		MeshExpressionPart::Transformation transform;
		transform.mPosition = Vector3(lmaFacialTransformation.mPosition.mX, lmaFacialTransformation.mPosition.mY, lmaFacialTransformation.mPosition.mZ);
		transform.mScale	= Vector3(lmaFacialTransformation.mScale.mX, lmaFacialTransformation.mScale.mY, lmaFacialTransformation.mScale.mZ);
		transform.mRotation	= Quaternion(lmaFacialTransformation.mRotation.mX, lmaFacialTransformation.mRotation.mY, lmaFacialTransformation.mRotation.mZ, lmaFacialTransformation.mRotation.mW);

		// add the transformation to the bones expression part
		expressionPart->AddTransformation(node->GetID(), transform);
	}

	// read the phoneme sets
	if (GetLogging()) LOG("    - Reading phoneme sets...");
	MCore::Array<ExpressionPart::EPhonemeSet> phonemeSets;
	phonemeSets.Resize( lmaExpressionPart.mNumPhonemeSets );
	for (int i=0; i<lmaExpressionPart.mNumPhonemeSets; i++)
	{
		int set;
		file->Read(&set, sizeof(int));
		phonemeSets[i] = (ExpressionPart::EPhonemeSet)set;
	}

	// set the read phoneme sets
	expressionPart->SetPhonemeSets( phonemeSets );

	return true;
}



//=================================================================================================


// constructor
ExpressionMotionPartChunkProcessor1::ExpressionMotionPartChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_EXPRESSIONMOTIONPART, 1)
{
}


// destructor
ExpressionMotionPartChunkProcessor1::~ExpressionMotionPartChunkProcessor1()
{
}


bool ExpressionMotionPartChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!motion)
	{
		if (GetLogging())
			LOG("Passed motion seems to be not valid!");

		return false;
	}

	// cast motion
	FacialMotion* facialMotion = (FacialMotion*)motion;

	// get the expression motion part
	LMA_ExpressionMotionPart lmaExpressionPart;
	file->Read(&lmaExpressionPart, sizeof(LMA_ExpressionMotionPart));

	// create a new facial motion part and set it's id
	int id = NodeIDGenerator::GetInstance().GenerateIDForString(lmaExpressionPart.mName);
	FacialMotionPart* motionPart = new FacialMotionPart(id);

	// get the part's keytrack
	KeyTrack<float, MCore::Compressed8BitFloat>* keytrack = motionPart->GetKeyTrack();

	// set the interpolator(always using hermite interpolation for facial animation)
	keytrack->SetInterpolator( new HermiteInterpolator<float, MCore::Compressed8BitFloat>() );

	if (GetLogging()) LOG("    - Expression Motion Part: %s", lmaExpressionPart.mName);
	if (GetLogging()) LOG("       + Interpolation Type = Hermite");
	if (GetLogging()) LOG("       + NrKeys             = %d", lmaExpressionPart.mNrKeys);

	// add keyframes
	for (int i=0; i<lmaExpressionPart.mNrKeys; i++)
	{
		LMA_UnsignedShortKey	lmaKey;

		// read the keyframe
		file->Read(&lmaKey, sizeof(LMA_UnsignedShortKey));

		// gives it into range of 0..1
		float normalizedValue = lmaKey.mValue / (float)65535;

		// add the keyframe to the keytrack
		keytrack->AddKey( lmaKey.mTime, normalizedValue );
	}

	keytrack->SetLoopMode( EMotionFX::KeyTrack<float, MCore::Compressed8BitFloat>::KEYTRACK_NO_LOOP );

	// init the keytrack
	keytrack->Init();

	// add the expression motion part
	facialMotion->AddExpMotionPart(motionPart);

	// each keytrack which stops before maxtime gets an ending key with time=maxTime.
	PhonemeMotionDataChunkProcessor1::SyncMotionTrackEnds(facialMotion);

	return true;
}


//=================================================================================================


// constructor
PhonemeMotionDataChunkProcessor1::PhonemeMotionDataChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_PHONEMEMOTIONDATA, 1)
{
}


// destructor
PhonemeMotionDataChunkProcessor1::~PhonemeMotionDataChunkProcessor1()
{
}


bool PhonemeMotionDataChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	if (!motion)
	{
		if (GetLogging())
			LOG("Passed motion seems to be not valid!");

		return false;
	}

	// cast motion
	FacialMotion* facialMotion = (FacialMotion*)motion;

	// get the facial motion part
	LMA_PhonemeMotionData lmaPhonemePart;
	file->Read(&lmaPhonemePart, sizeof(LMA_PhonemeMotionData));

	if (GetLogging()) LOG("    - Phoneme Motion Data");
	if (GetLogging()) LOG("       + Interpolation Type = Hermite");
	if (GetLogging()) LOG("       + NrKeys             = %d", lmaPhonemePart.mNrKeys);
	if (GetLogging()) LOG("       + NumPhonemes        = %d", lmaPhonemePart.mNumPhonemes);

	// get the number of phonemes
	int numPhonemes = lmaPhonemePart.mNumPhonemes;

	int i;
	// get all phoneme names
	for (i=0; i<numPhonemes; i++)
	{
		char phonemeName[40];

		// read the name
		file->Read(phonemeName, sizeof(char) * 40);
		if (GetLogging()) LOG("          - Phoneme             = %s", phonemeName);

		// create a new facial motion part and set it's id
		int id = NodeIDGenerator::GetInstance().GenerateIDForString(phonemeName);
		FacialMotionPart* motionPart = new FacialMotionPart(id);

		// get the part's keytrack
		KeyTrack<float, MCore::Compressed8BitFloat>* keytrack = motionPart->GetKeyTrack();

		// set the interpolator(always using hermite interpolation for facial animation)
		keytrack->SetInterpolator( new HermiteInterpolator<float, MCore::Compressed8BitFloat>() );

		keytrack->SetLoopMode( EMotionFX::KeyTrack<float, MCore::Compressed8BitFloat>::KEYTRACK_NO_LOOP );

		// add the phoneme motion part
		facialMotion->AddPhoMotionPart(motionPart);
	}

	// if assert fails, something strange has happened
	assert(numPhonemes == facialMotion->GetNumPhoMotionParts());

	// iterate through all keyframes
	for (i=0; i<lmaPhonemePart.mNrKeys; i++)
	{
		LMA_PhonemeKey lmaKey;

		// read the keyframe
		file->Read(&lmaKey, sizeof(LMA_PhonemeKey));

		//LOG("          - Key: Time=%f, PhoNum=%i, Power=%i", lmaKey.mTime, lmaKey.mPhonemeNumber, lmaKey.mPowerValue);

		// iterate through all phoneme keytracks and add the given keyframe
		for (int j=0; j<facialMotion->GetNumPhoMotionParts(); j++)
		{
			// get the part's keytrack
			KeyTrack<float, MCore::Compressed8BitFloat>* keytrack = facialMotion->GetPhoKeyTrack(j);

			// if the first key isn't at the beginning of the animation, so at time=0.0
			// add a keyframe in front of them all
			if (i == 0 && lmaKey.mTime > 0.0)
			{
				// add it sorted since the keys are stored in the order the artist adds them in lmstudio
				keytrack->AddKeySorted( 0, 0 );
			}

			float keyTime  = lmaKey.mTime;
			float keyValue;

			// check if this key belongs to the current phoneme
			// if yes set the power value, if no reset it to 0.0 (phoneme pose reset)
			if (j == lmaKey.mPhonemeNumber)
				keyValue = lmaKey.mPowerValue / (float)255.0f;
			else
				keyValue = 0;

			// add it sorted since the keys are stored in the order the artist adds them in lmstudio
			keytrack->AddKeySorted(keyTime, keyValue);
		}
	}

	// init all keytracks
	for (i=0; i<facialMotion->GetNumPhoMotionParts(); i++)
	{
		// get the part's keytrack
		KeyTrack<float, MCore::Compressed8BitFloat>* keytrack = facialMotion->GetPhoKeyTrack(i);

		// init the keytrack
		if (keytrack)
			keytrack->Init();
	}

	// each keytrack which stops before maxtime gets an ending key with time=maxTime.
	PhonemeMotionDataChunkProcessor1::SyncMotionTrackEnds(facialMotion);

	return true;
}


// checks if the motion parts stop at the same time if not add some helper key
void PhonemeMotionDataChunkProcessor1::SyncMotionTrackEnds(FacialMotion* facialMotion)
{
	// the maximum time of the animation
	float maxTime = FindMaxTime(facialMotion);

	// iterate through all phoneme keytracks
	int i;
	for (i=0; i<facialMotion->GetNumPhoMotionParts(); i++)
	{
		// get the part's keytrack
		KeyTrack<float, MCore::Compressed8BitFloat>* keytrack = facialMotion->GetPhoKeyTrack(i);

		// add an ending key if the track stops before maxTime
		if (keytrack)
		{
			if (keytrack->GetNumKeys() > 0)
			{
				if (keytrack->GetLastKey()->GetTime() < maxTime)
				{
					// add it sorted since the keys are stored in the order the artist adds them in lmstudio
					keytrack->AddKeySorted(maxTime, 0);
				}
			}
		}
	}

	// iterate through all expression keytracks
	for (i=0; i<facialMotion->GetNumExpMotionParts(); i++)
	{
		// get the part's keytrack
		KeyTrack<float, MCore::Compressed8BitFloat>* keytrack = facialMotion->GetExpKeyTrack(i);

		// add an ending key if the track stops before maxTime
		if (keytrack)
		{
			if (keytrack->GetNumKeys() > 0)
			{
				if (keytrack->GetLastKey()->GetTime() < maxTime)
				{
					// add it sorted since the keys are stored in the order the artist adds them in lmstudio
					keytrack->AddKeySorted(maxTime, 0);
				}
			}
		}
	}
}


// return the maximum time of the animation
float PhonemeMotionDataChunkProcessor1::FindMaxTime(FacialMotion* facialMotion)
{
	// the maximum time of the animation
	float maxTime = 0.0;

	// iterate through all phoneme keytracks
	int i;
	for (i=0; i<facialMotion->GetNumPhoMotionParts(); i++)
	{
		// get the part's keytrack
		KeyTrack<float, MCore::Compressed8BitFloat>* keytrack = facialMotion->GetPhoKeyTrack(i);

		// check if the maxtime isn't the real maxtime anymore and replace if in that case
		if (keytrack)
			if (keytrack->GetNumKeys() > 0)
				if (keytrack->GetLastKey()->GetTime() > maxTime)
					maxTime = keytrack->GetLastKey()->GetTime();
	}

	// iterate through all expression keytracks
	for (i=0; i<facialMotion->GetNumExpMotionParts(); i++)
	{
		// get the part's keytrack
		KeyTrack<float, MCore::Compressed8BitFloat>* keytrack = facialMotion->GetExpKeyTrack(i);

		// check if the maxtime isn't the real maxtime anymore and replace if in that case
		if (keytrack)
			if (keytrack->GetNumKeys() > 0)
				if (keytrack->GetLastKey()->GetTime() > maxTime)
					maxTime = keytrack->GetLastKey()->GetTime();
	}

	// return the maximum time of the facial animation
	return maxTime;
}


//=================================================================================================


// constructor
FXMaterialChunkProcessor1::FXMaterialChunkProcessor1(Importer* lmaImporter) : ChunkProcessor(lmaImporter, LMA_CHUNK_FXMATERIAL, 1)
{
}


// destructor
FXMaterialChunkProcessor1::~FXMaterialChunkProcessor1()
{
}


bool FXMaterialChunkProcessor1::Process(MCore::File* file, Actor* actor, Motion* motion, bool usePerPixelLighting)
{
	DECLARE_FUNCTION(Process)

	// validate the actor
	if (!actor)
	{
		if (GetLogging())
			LOG("Passed actor seems to be not valid!");

		return false;
	}

	// read the header
	LMA_FXMaterial material;
	file->Read(&material, sizeof(LMA_FXMaterial));

	// print material information
	if (GetLogging()) LOG("- Material name = '%s'", material.mName);
	if (GetLogging()) LOG("  + Effect file       = '%s'", material.mEffectFile);
	if (GetLogging()) LOG("  + Num int params    = %d", material.mNumIntParams);
	if (GetLogging()) LOG("  + Num float params  = %d", material.mNumFloatParams);
	if (GetLogging()) LOG("  + Num color params  = %d", material.mNumColorParams);
	if (GetLogging()) LOG("  + Num bitmap params = %d", material.mNumBitmapParams);


	// create the EMFX FX material and add it to the actor
	MCore::Pointer<Material> mat( new FXMaterial(material.mName) );
	FXMaterial* fxMat = (FXMaterial*)mat.GetPointer();
	actor->AddMaterial(0, mat);

	// setup the material values
	fxMat->SetEffectFile( material.mEffectFile );


	// read the int parameters
	if (GetLogging()) LOG("  + Parameters:");
	int i;
	for (i=0; i<material.mNumIntParams; i++)
	{
		LMA_FXIntParameter fileParam;
		file->Read(&fileParam, sizeof(LMA_FXIntParameter));
		if (GetLogging()) LOG("    + int %s = %d", fileParam.mName, fileParam.mValue);
		fxMat->AddIntParameter(fileParam.mName, fileParam.mValue);
	}

	// read the float parameters
	for (i=0; i<material.mNumFloatParams; i++)
	{
		LMA_FXFloatParameter fileParam;
		file->Read(&fileParam, sizeof(LMA_FXFloatParameter));
		if (GetLogging()) LOG("    + float %s = %f", fileParam.mName, fileParam.mValue);
		fxMat->AddFloatParameter(fileParam.mName, fileParam.mValue);
	}

	// read the color parameters
	for (i=0; i<material.mNumColorParams; i++)
	{
		LMA_FXColorParameter fileParam;
		file->Read(&fileParam, sizeof(LMA_FXColorParameter));
		if (GetLogging()) LOG("    + color %s = (%f, %f, %f, %f)", fileParam.mName, fileParam.mR, fileParam.mG, fileParam.mB, fileParam.mA);
		fxMat->AddColorParameter(fileParam.mName, MCore::RGBAColor(fileParam.mR, fileParam.mG, fileParam.mB, fileParam.mA));
	}

	// read the bitmap parameters
	for (i=0; i<material.mNumBitmapParams; i++)
	{
		LMA_FXBitmapParameter fileParam;
		file->Read(&fileParam, sizeof(LMA_FXBitmapParameter));
		if (GetLogging()) LOG("    + string %s = %s", fileParam.mName, fileParam.mFilename);
		fxMat->AddStringParameter(fileParam.mName, fileParam.mFilename);
	}

	return true;
}




} // namespace EMotionFX