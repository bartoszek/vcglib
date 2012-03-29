/****************************************************************************
* VCGLib                                                            o o     *
* Visual and Computer Graphics Library                            o     o   *
*                                                                _   O  _   *
* Copyright(C) 2004                                                \/)\/    *
* Visual Computing Lab                                            /\/|      *
* ISTI - Italian National Research Council                           |      *
*                                                                    \      *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *   
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
****************************************************************************/
#include <vcg/math/matrix33.h>
#include <vcg/math/histogram.h>
#include <vcg/complex/algorithms/update/curvature.h>
#include <vcg/simplex/face/topology.h>

#ifndef VCG_TANGENT_FIELD_OPERATORS
#define VCG_TANGENT_FIELD_OPERATORS

namespace vcg {
	namespace tri{

		template <class MeshType>
		class CrossField
		{
			typedef typename MeshType::FaceType FaceType;
			typedef typename MeshType::VertexType VertexType;
			typedef typename MeshType::CoordType CoordType;
			typedef typename MeshType::ScalarType ScalarType;

		private:
			static ScalarType Sign(ScalarType a){return (ScalarType)((a>0)?+1:-1);}

		public:


			static void SetVertCrossFromCurvature(MeshType &mesh)
			{
				vcg::tri::UpdateTopology<MeshType>::FaceFace(mesh);
				vcg::tri::UpdateTopology<MeshType>::VertexFace(mesh);
				vcg::tri::UpdateBounding<MeshType>::Box(mesh);

				//set as selected high curvature value
				vcg::tri::UpdateCurvature<MeshType>::PrincipalDirectionsNormalCycles(mesh);
				NormalizePerVertImportanceVal(mesh);
				///save the curvature value
				std::vector<ScalarType> K1,K2;
				K1.resize(mesh.vert.size());
				K2.resize(mesh.vert.size());
				for (int j=0;j<mesh.vert.size();j++)
				{
					VertexType *v=&mesh.vert[j];
					if(v->IsD())continue;
					K1[j]=v->K1();
					K2[j]=v->K2();
				}
				///then find multiscale curvature directions
				vcg::tri::UpdateCurvature<MeshType>::PrincipalDirectionsPCA(mesh,mesh.bbox.Diag()/200.0);
				///and save back importance val
				for (int j=0;j<mesh.vert.size();j++)
				{
					VertexType *v=&mesh.vert[j];
					if(v->IsD())continue;
					v->K1()=K1[j];
					v->K2()=K2[j];
				}

				///set normal according to curvature
				for (int j=0;j<mesh.vert.size();j++)
				{
					VertexType *v=&mesh.vert[j];
					if(v->IsD())continue;
					CoordType N0=v->N();
					v->N()=v->PD1()^v->PD2();
					v->N().Normalize();
					if (N0*v->N()<0)
						v->N()=-v->N();
				}
			}
			
		
			
			///fird a tranformation matrix to transform 
			///the 3D space to 2D tangent space specified 
			///by the cross field (where Z=0)
			static vcg::Matrix33<ScalarType> TransformationMatrix(const FaceType &f)
			{
				typedef typename FaceType::CoordType CoordType;
				typedef typename FaceType::ScalarType ScalarType;

				///transform to 3d
				CoordType axis0=f.cPD1();
				CoordType axis1=f.cPD2();//axis0^f.cN();
				CoordType axis2=f.cN();

				return (vcg::TransformationMatrix(axis0,axis1,axis2));
			}

			
			///transform a given angle in tangent space wrt X axis of
			///tangest space will return the corresponding 3D vector
			static CoordType TangetAngleToVect(const FaceType &f,const ScalarType &angle)
			{
				///find 2D vector
				vcg::Point2<ScalarType> axis2D=vcg::Point2<ScalarType>(cos(angle),sin(angle));
				CoordType axis3D=CoordType(axis2D.X(),axis2D.Y(),0);
				vcg::Matrix33<ScalarType> Trans=TransformationMatrix(f);
				vcg::Matrix33<ScalarType> InvTrans=Inverse(Trans);
				///then transform
				return (InvTrans*axis3D);
			}
			
			///find an angle with respect to dirX on the plane perpendiculr to DirZ
			///dirX and dirZ should be perpendicular
			static ScalarType TangentVectToAngle(const CoordType dirX,
											const CoordType dirZ,
											const CoordType &vect3D)
			{
				const CoordType dirY=dirX^dirZ;
				dirX.Normalize();
				dirY.Normalize();
				dirZ.Normalize();
				vcg::Matrix33<ScalarType> Trans=TransformationMatrix(dirX,dirY,dirZ);
				///trensform the vector to the reference frame by rotating it
				CoordType vect_transf=Trans*vect3D;

				///then put to zero to the Z coordinate
				vcg::Point2<ScalarType> axis2D=vcg::Point2<ScalarType>(vect_transf.X(),vect_transf.Y());
				axis2D.Normalize();

				///then find the angle with respact to axis 0
				ScalarType alpha=atan2(axis2D.Y(),axis2D.X());	////to sum up M_PI?
				if (alpha<0)
					alpha=(2*M_PI+alpha);
				if (alpha<0)
					alpha=0;
				return alpha;
			}

			///find an angle with respect to the tangent frame of given face
			static ScalarType VectToAngle(const FaceType &f,const CoordType &vect3D)
			{
				vcg::Matrix33<ScalarType> Trans=TransformationMatrix(f);

				///trensform the vector to the reference frame by rotating it
				CoordType vect_transf=Trans*vect3D;

				///then put to zero to the Z coordinate
				vcg::Point2<ScalarType> axis2D=vcg::Point2<ScalarType>(vect_transf.X(),vect_transf.Y());
				axis2D.Normalize();

				///then find the angle with respact to axis 0
				ScalarType alpha=atan2(axis2D.Y(),axis2D.X());	////to sum up M_PI?
				if (alpha<0)
					alpha=(2*M_PI+alpha);
				if (alpha<0)
					alpha=0;
				return alpha;
			}

			///return the 4 directiona of the cross field in 3D
			///given a first direction as input
			static void CrossVector(const CoordType &dir0,
									const CoordType &norm,
									CoordType axis[4])
			{
				axis[0]=dir0;
				axis[1]=norm^axis[0];
				axis[2]=-axis[0];
				axis[3]=-axis[1];
			}

			///return the 4 direction in 3D of 
			///the cross field of a given face
			static void CrossVector(const FaceType &f,
									CoordType axis[4])
			{
				CoordType dir0=f.cPD1();
				CoordType dir1=f.cPD2();
				axis[0]=dir0;
				axis[1]=dir1;
				axis[2]=-dir0;
				axis[3]=-dir1;
			}

			///return a specific direction given an integer 0..3
			///considering the reference direction of the cross field
			static CoordType CrossVector(const FaceType &f,
										const int &index)
			{
				assert((index>=0)&&(index<4));
				CoordType axis[4];
				CrossVector(f,axis);
				return axis[index];
			}

			///set the cross field of a given face
			static void SetCrossVector(FaceType &f,
										CoordType dir0,
										CoordType dir1)
			{
				f.PD1()=dir0;
				f.PD2()=dir1;
			}

			///set the face cross vector from vertex one
			static void SetFaceCrossVectorFromVert(FaceType &f)
			{
				const CoordType &t0=f.V(0)->PD1();
				const CoordType &t1=f.V(1)->PD1();
				const CoordType &t2=f.V(2)->PD1();
				const CoordType &N0=f.V(0)->N();
				const CoordType &N1=f.V(0)->N();
				const CoordType &N2=f.V(0)->N();
				const CoordType &NF=f.N();
				const CoordType bary=CoordType(0.33333,0.33333,0.33333);
				CoordType tF0,tF1;
				tF0=InterpolateCrossField(t0,t1,t2,N0,N1,N2,NF,bary);
				tF1=NF^tF0;
				tF0.Normalize();
				tF1.Normalize();
				SetCrossVector(f,tF0,tF1);
			}

			static void SetFaceCrossVectorFromVert(MeshType &mesh)
			{
				for (int i=0;i<mesh.face.size();i++)
				{
					FaceType *f=&mesh.face[i];
					if (f->IsD())continue;
					SetFaceCrossVectorFromVert(*f);
				}
			}
			

			///rotate a given vector from the tangent space
			///of f0 to the tangent space of f1 by considering the difference of normals
			static CoordType Rotate(const FaceType &f0,const FaceType &f1,const CoordType &dir3D)
			{
				CoordType N0=f0.cN();
				CoordType N1=f1.cN();

				///find the rotation matrix that maps between normals
				vcg::Matrix33<ScalarType> rotation=vcg::RotationMatrix(N0,N1);
				CoordType rotated=rotation*dir3D;
				return rotated;
			}

			// returns the 90 deg rotation of a (around n) most similar to target b
			/// a and b should be in the same plane orthogonal to N
			static CoordType K_PI(const CoordType &a, const CoordType &b, const CoordType &n)
			{
				CoordType c = (a^n).normalized();
				ScalarType scorea = a*b;
				ScalarType scorec = c*b;
				if (fabs(scorea)>=fabs(scorec)) return a*Sign(scorea); else return c*Sign(scorec);
			}

			///interpolate cross field with barycentric coordinates
			static CoordType InterpolateCrossField(const CoordType &t0,
													const CoordType &t1,
													const CoordType &t2,
													const CoordType &n0,
													const CoordType &n1,
													const CoordType &n2,
													const CoordType &target_n,
													const CoordType &bary)
			{
				vcg::Matrix33<ScalarType> R0=vcg::RotationMatrix(n0,target_n);
				vcg::Matrix33<ScalarType> R1=vcg::RotationMatrix(n1,target_n);
				vcg::Matrix33<ScalarType> R2=vcg::RotationMatrix(n2,target_n);
				///rotate
				CoordType trans0=R0*t0;
				CoordType trans1=R1*t1;
				CoordType trans2=R2*t2;
				///normalize it
				trans0.Normalize();
				trans1.Normalize();
				trans2.Normalize();
				///k_PI/2 rotation
				trans1=K_PI(trans1,trans0,target_n);
				trans2=K_PI(trans2,trans0,target_n);
				trans1.Normalize();
				trans2.Normalize();

				CoordType sum = trans0*bary.X() + trans1 * bary.Y() + trans2 * bary.Z();
				return sum;
			}

			///interpolate cross field with barycentric coordinates using normalized weights
			static typename typename CoordType InterpolateCrossField(const std::vector<CoordType> &TangVect,
																	const std::vector<ScalarType> &Weight,
																	const std::vector<CoordType> &Norms,
																	const typename CoordType &BaseNorm,
																	const typename CoordType &BaseDir)
			{
				typedef typename FaceType::CoordType CoordType;
				typedef typename FaceType::ScalarType ScalarType;

				CoordType sum = CoordType(0,0,0);
				for (int i=0;i<TangVect.size();i++)
				{
					CoordType N1=Norms[i];
					///find the rotation matrix that maps between normals
					vcg::Matrix33<ScalarType> rotation=vcg::RotationMatrix(N1,BaseNorm);
					CoordType rotated=rotation*TangVect[i];
					CoordType Tdir=K_PI(rotated,BaseDir,BaseNorm);
					Tdir.Normalize();
					sum+=(Tdir*Weight[i]);
				}
				sum.Normalize();
				return sum;
			}

			///interpolate cross field with scalar weight
			static typename FaceType::CoordType InterpolateCrossFieldLine(const typename FaceType::CoordType &t0,
																		const typename FaceType::CoordType &t1,
																		const typename FaceType::CoordType &n0,
																		const typename FaceType::CoordType &n1,
																		const typename FaceType::CoordType &target_n,
																		const typename FaceType::ScalarType &weight)
			{
				vcg::Matrix33<ScalarType> R0=vcg::RotationMatrix(n0,target_n);
				vcg::Matrix33<ScalarType> R1=vcg::RotationMatrix(n1,target_n);
				CoordType trans0=R0*t0;
				CoordType trans1=R1*t1;
				//CoordType trans0=t0;//R0*t0;
				//CoordType trans1=t1;//R1*t1;
				trans0.Normalize();
				trans1.Normalize();
				trans1=K_PI(trans1,trans0,target_n);
				trans1.Normalize();
				CoordType sum = trans0*weight + trans1 * (1.0-weight);
				return sum;
			}


			///return the difference of two cross field, values between [0,0.5]
			static typename FaceType::ScalarType DifferenceCrossField(const typename FaceType::CoordType &t0,
																	const typename FaceType::CoordType &t1,
																	const typename FaceType::CoordType &n)
			{
				CoordType trans0=t0;
				CoordType trans1=K_PI(t1,t0,n);
				ScalarType diff = 1-fabs(trans0*trans1);
				return diff;
			}
			
			///return the difference of two cross field, values between [0,0.5]
			static typename FaceType::ScalarType DifferenceCrossField(const typename vcg::Point2<ScalarType> &t0,
																const typename vcg::Point2<ScalarType> &t1)
			{
				CoordType t03D=CoordType(t0.X(),t0.Y(),0);
				CoordType t13D=CoordType(t1.X(),t1.Y(),0);
				CoordType trans0=t03D;
				CoordType n=CoordType(0,0,1);
				CoordType trans1=K_PI(t13D,t03D,n);
				ScalarType diff = 1-fabs(trans0*trans1);
				return diff;
			}

			///compute the mismatch between 2 directions 
			///each one si perpendicular to its own normal
			static int MissMatch(const CoordType &dir0,
								 const CoordType &dir1,
								 const CoordType &N0,
								 const CoordType &N1)
			{
				CoordType dir0Rot=Rotate(dir0,N0,N1);
				CoordType dir1Rot=dir1;

				dir0Rot.Normalize();
				dir1Rot.Normalize();
				
				ScalarType angle_diff=VectToAngle(dir0Rot,N0,dir1Rot);

				ScalarType step=M_PI/2.0;
				int i=(int)floor((angle_diff/step)+0.5);
				int k=0;
				if (i>=0)
					k=i%4;
				else
					k=(-(3*i))%4;
				return k;
			}

			///compute the mismatch between 2 faces
			static int MissMatch(const FaceType &f0,
								const FaceType &f1)
			{
				CoordType dir0=CrossVector(f0,0);
				CoordType dir1=CrossVector(f1,0);

				CoordType dir1Rot=Rotate(f1,f0,dir1);
				dir1Rot.Normalize();

				ScalarType angle_diff=VectToAngle(f0,dir1Rot);

				ScalarType step=M_PI/2.0;
				int i=(int)floor((angle_diff/step)+0.5);
				int k=0;
				if (i>=0)
					k=i%4;
				else
					k=(-(3*i))%4;
				return k;
			}
			
			
			///return true if a given vertex is singular,
			///return also the missmatch
			static bool IsSingular(VertexType &v,int &missmatch)
			{
				typedef typename VertexType::FaceType FaceType;
				///check that is on border..
				if (v.IsB())return false;

				std::vector<FaceType*> faces;
				//SortedFaces(v,faces);
				vcg::face::VFOrderedStarVF_FF(v,faces);

				missmatch=0;
				for (int i=0;i<faces.size();i++)
				{
					FaceType *curr_f=faces[i];
					FaceType *next_f=faces[(i+1)%faces.size()];

					///find the current missmatch
					missmatch+=MissMatch(*curr_f,*next_f);

				}
				missmatch=missmatch%4;
				return(missmatch!=0);
			}
			
			///select singular vertices
			static void SelectSingular(MeshType &mesh)
			{
				for (int i=0;i<mesh.vert.size();i++)
				{
					if (mesh.vert[i].IsD())continue;
					int missmatch;
					if (IsSingular(mesh.vert[i],missmatch))
						mesh.vert[i].SetS();
					else
						mesh.vert[i].ClearS();
				}
			}
			
			///load a field on the mesh, it could be a vfield file (per vertex)
			///or an ffield file (per face)
			static bool LoadFIELD(MeshType *mesh,
								const char *path,
								bool per_vertex=false)
			{
				
				FILE *f = fopen(path,"rt");
				if (!f)
				{
					return false;
				}
				{
					char word[512]; word[0]=0;
					fscanf(f,"%s",word);
					char c=0;
					if (word[0]=='#') {
						// skip comment line
						while (fscanf(f,"%c",&c)!=EOF) if (c=='\n') break;
					} 
					else 
					{
						return false;
					}
					int nnv = -1;
					if (fscanf(f,"%d",&nnv)!=1) 
					{
						while (fscanf(f,"%c",&c)!=EOF) if (c=='\n') break; // skip
						fscanf(f,"%d",&nnv);
					}
					int targetnum=mesh->fn;
					if (per_vertex)
						targetnum=mesh->vn;
					if (nnv != (int)targetnum) 
					{
						//if (errorMsg) sprintf(errorMsg,"Wrong element number. Found: %d. Expected: %d.",nnv,mesh->vn);
						return false;
					}
					while (fscanf(f,"%c",&c)!=EOF) if (c=='\n') break; // skip
					// skip strange string line
					while (fscanf(f,"%c",&c)!=EOF) if (c=='\n') break;
					for (int i=0; i<nnv; i++){
						vcg::Point3d u,v;
						int a,b;
						if (fscanf(f,
							"%d %d %lf %lf %lf %lf %lf %lf",
							&a,&b,
							&(v.X()),&(v.Y()),&(v.Z()),
							&(u.X()),&(u.Y()),&(u.Z())
							)!=8) {
								//if (errorMsg) sprintf(errorMsg,"Format error reading vertex n. %d",i);
								return false;
						}
						//node[i]->TF().Import(u);
						if (per_vertex)
						{
							mesh->vert[i].PD1()=u;
							mesh->vert[i].PD2()=v;
						}
						else
						{
							mesh->face[i].PD1()=u;
							mesh->face[i].PD2()=v;
						}
					}
				}
				fclose(f);
				return true;
			}

			///transform curvature to UV space
			static vcg::Point2<ScalarType> CrossToUV(FaceType &f)
			{
				typedef typename FaceType::ScalarType ScalarType;
				typedef typename FaceType::CoordType CoordType;

				CoordType Curv=CrossVector(f,0);
				Curv.Normalize();

				CoordType bary3d=(f.P(0)+f.P(1)+f.P(2))/3.0;
				vcg::Point2<ScalarType> Uv0=f.V(0)->T().P();
				vcg::Point2<ScalarType> Uv1=f.V(1)->T().P();
				vcg::Point2<ScalarType> Uv2=f.V(2)->T().P();
				vcg::Point2<ScalarType> baryUV=(Uv0+Uv1+Uv2)/3.0;
				CoordType direct3d=bary3d+Curv;
				CoordType baryCoordsUV;
				vcg::InterpolationParameters<FaceType,ScalarType>(f,direct3d,baryCoordsUV);
				vcg::Point2<ScalarType> curvUV=baryCoordsUV.X()*Uv0+
												baryCoordsUV.Y()*Uv1+
												baryCoordsUV.Z()*Uv2-baryUV;
				curvUV.Normalize();
				return curvUV;
			}

		};///end class
	} //End Namespace Tri
} // End Namespace vcg
#endif