/////////////////////////////////////////////////////////////////////////////
// Name:            Maximas.cpp
// Author:          Maxime Chamberland
/////////////////////////////////////////////////////////////////////////////

#include "Maximas.h"

#include "DatasetManager.h"
#include "../Logger.h"
#include "../gfx/ShaderHelper.h"
#include "../gui/MyListCtrl.h"
#include "../gui/SceneManager.h"
#include "../misc/nifti/nifti1_io.h"

#include <GL/glew.h>
#include <wx/math.h>
#include <wx/xml/xml.h>

#include <algorithm>
#include <fstream>
#include <vector>
using std::vector;

#ifndef isnan
inline bool isnan(double x) {
    return x != x;
}
#endif


///////////////////////////////////////////
Maximas::Maximas( const wxString &filename )
: Glyph()
{
    m_fullPath = filename;
    m_scalingFactor = 5.0f;


#ifdef __WXMSW__
    m_name = filename.AfterLast( '\\' );
#else
    m_name = filename.AfterLast( '/' );
#endif

}

//////////////////////////////////////////////////////////////////////////
Maximas::~Maximas()
{
    Logger::getInstance()->print( wxT( "Maximas destructor called but nothing to do." ), LOGLEVEL_DEBUG );
}

//////////////////////////////////////////////////////////////////////////
bool Maximas::load( nifti_image *pHeader, nifti_image *pBody )
{
    m_columns = pHeader->dim[1]; //XSlice
    m_rows    = pHeader->dim[2]; //YSlice
    m_frames  = pHeader->dim[3]; //ZSlice
    m_bands   = pHeader->dim[4]; //9

    m_voxelSizeX = pHeader->dx;
    m_voxelSizeY = pHeader->dy;
    m_voxelSizeZ = pHeader->dz;

    float voxelX = DatasetManager::getInstance()->getVoxelX();
    float voxelY = DatasetManager::getInstance()->getVoxelY();
    float voxelZ = DatasetManager::getInstance()->getVoxelZ();

    if( m_voxelSizeX != voxelX || m_voxelSizeY != voxelY || m_voxelSizeZ != voxelZ )
    {
        Logger::getInstance()->print( wxT( "Voxel size different from anatomy." ), LOGLEVEL_ERROR );
        return false;
    }

    m_type = MAXIMAS;

    int datasetSize = pHeader->dim[1] * pHeader->dim[2] * pHeader->dim[3];
    std::vector< float > l_fileFloatData( datasetSize * m_bands );

    float* pData = (float*)pBody->data;

    for( int i( 0 ); i < datasetSize; ++i )
    {
        for( int j( 0 ); j < m_bands; ++j )
        {
            if(!isnan(pData[j * datasetSize + i]))
                l_fileFloatData[i * m_bands + j] = pData[j * datasetSize + i];
        }
    }
    
    createStructure( l_fileFloatData );

    m_isLoaded = true;
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool Maximas::createStructure  ( std::vector< float > &i_fileFloatData )
{
    m_nbGlyphs         = DatasetManager::getInstance()->getColumns() * DatasetManager::getInstance()->getRows() * DatasetManager::getInstance()->getFrames();
    m_mainDirections.resize( m_nbGlyphs );

    vector< float >::iterator it;
    int i = 0;

    //Fetching the directions
    for( it = i_fileFloatData.begin(), i = 0; it != i_fileFloatData.end(); it += m_bands, ++i )
    { 
        if(*it != NULL)
            m_mainDirections[i].insert( m_mainDirections[i].end(), it, it + m_bands );
    }

    getSlidersPositions( m_currentSliderPos );

    return true;
}

//////////////////////////////////////////////////////////////////////////
void Maximas::draw()
{
    // Enable the shader.
    ShaderHelper::getInstance()->getOdfsShader()->bind();
    glBindTexture( GL_TEXTURE_1D, m_textureId );

    // This is the color look up table texture.
    ShaderHelper::getInstance()->getOdfsShader()->setUniSampler( "clut", 0 );
    
    // This is the brightness level of the odf.
    ShaderHelper::getInstance()->getOdfsShader()->setUniFloat( "brightness", DatasetInfo::m_brightness );

    // This is the alpha level of the odf.
    ShaderHelper::getInstance()->getOdfsShader()->setUniFloat( "alpha", DatasetInfo::m_alpha );

    // If m_colorWithPosition is true then the glyph will be colored with the position of the vertex.
    ShaderHelper::getInstance()->getOdfsShader()->setUniInt( "colorWithPos", ( GLint ) m_colorWithPosition );
 
    Glyph::draw();

    // Disable the tensor color shader.
    ShaderHelper::getInstance()->getOdfsShader()->release();
}

//////////////////////////////////////////////////////////////////////////
void Maximas::setScalingFactor( float i_scalingFactor )
{
    m_scalingFactor = i_scalingFactor;
    generateSpherePoints( m_scalingFactor/5 );   
    loadBuffer();
}

//////////////////////////////////////////////////////////////////////////
void Maximas::drawGlyph( int i_zVoxel, int i_yVoxel, int i_xVoxel, AxisType i_axis )
{
    if( ! boxInFrustum( Vector( ( i_xVoxel + 0.5f ) * m_voxelSizeX,
                                ( i_yVoxel + 0.5f ) * m_voxelSizeY,
                                ( i_zVoxel + 0.5f ) * m_voxelSizeZ ),
                        Vector( m_voxelSizeX * 0.5f,
                                m_voxelSizeY * 0.5f,
                                m_voxelSizeZ * 0.5f ) ) )
        return;

    // Get the current maxima index in the coeffs's buffer
    int  currentIdx = getGlyphIndex( i_zVoxel, i_yVoxel, i_xVoxel );   
 
    // Maxima offset..
    GLfloat l_offset[3];
    getVoxelOffset( i_zVoxel, i_yVoxel, i_xVoxel, l_offset );
    ShaderHelper::getInstance()->getOdfsShader()->setUni3Float( "offset", l_offset );

    GLfloat l_flippedAxes[3];
    m_flippedAxes[0] ? l_flippedAxes[0] = -1.0f : l_flippedAxes[0] = 1.0f;
    m_flippedAxes[1] ? l_flippedAxes[1] = -1.0f : l_flippedAxes[1] = 1.0f;
    m_flippedAxes[2] ? l_flippedAxes[2] = -1.0f : l_flippedAxes[2] = 1.0f;

    ShaderHelper::getInstance()->getOdfsShader()->setUni3Float(   "axisFlip",    l_flippedAxes );

    ShaderHelper::getInstance()->getOdfsShader()->setUniInt( "showAxis", 1 );

    if(m_mainDirections[currentIdx].size() != 0)
    { 
        for(unsigned int i =0; i < m_mainDirections[currentIdx].size()/3; i++)
        {
            GLfloat l_coloring[3];
            l_coloring[0] = m_mainDirections[currentIdx][i*3];
            l_coloring[1] = m_mainDirections[currentIdx][i*3+1];
            l_coloring[2] = m_mainDirections[currentIdx][i*3+2];
            ShaderHelper::getInstance()->getOdfsShader()->setUni3Float( "coloring", l_coloring );
            
            float halfScale = m_scalingFactor / 5.0f;
            GLfloat stickPos[3];
            stickPos[0] = halfScale*m_mainDirections[currentIdx][i*3];
            stickPos[1] = halfScale*m_mainDirections[currentIdx][i*3+1];
            stickPos[2] = halfScale*m_mainDirections[currentIdx][i*3+2];

            glBegin(GL_LINES);  
                glVertex3f(-stickPos[0],-stickPos[1],-stickPos[2]);
                glVertex3f(stickPos[0],stickPos[1],stickPos[2]);       
            glEnd();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void Maximas::createPropertiesSizer( PropertiesWindow *pParent )
{
    Glyph::createPropertiesSizer( pParent );
}

//////////////////////////////////////////////////////////////////////////
void Maximas::updatePropertiesSizer()
{
    Glyph::updatePropertiesSizer();
    
    m_pSliderLightAttenuation->Enable( false );
    m_pSliderLightXPosition->Enable( false );
    m_pSliderLightYPosition->Enable( false );
    m_pSliderLightZPosition->Enable( false );
    m_pBtnFlipX->Enable( false );
    m_pBtnFlipY->Enable( false );
    m_pBtnFlipZ->Enable( false );

    m_pSliderMinHue->Enable( false );
    m_pSliderMaxHue->Enable( false );
    m_pSliderSaturation->Enable( false );
    m_pSliderLuminance->Enable( false );
    m_pSliderLOD->Enable( false );
    m_pSliderDisplay->Enable( false );
    m_pSliderScalingFactor->SetValue( getScalingFactor() * 10.0f );

    m_pToggleAxisFlipX->SetValue( isAxisFlipped( X_AXIS ) );
    m_pToggleAxisFlipY->SetValue( isAxisFlipped( Y_AXIS ) );
    m_pToggleAxisFlipZ->SetValue( isAxisFlipped( Z_AXIS ) );
    m_pToggleColorWithPosition->Enable( false );

    m_pRadNormal->Enable( false );
    m_pRadMapOnSphere->Enable( false );
    m_pRadMainAxis->Enable( false );
}
