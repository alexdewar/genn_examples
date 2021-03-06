#include "route.h"

// Standard C++ includes
#include <fstream>
#include <iostream>
#include <limits>
#include <tuple>

// Standard C++ includes
#include <cmath>

// Antworld includes
#include "common.h"

// //----------------------------------------------------------------------------
// Anonymous namespace
//----------------------------------------------------------------------------
namespace
{
float sqr(float x)
{
    return (x * x);
}
//----------------------------------------------------------------------------
float distanceSquared(float x1, float y1, float x2, float y2)
{
    return sqr(x2 - x1) + sqr(y2 - y1);
}
}   // Anonymous namespace

//----------------------------------------------------------------------------
// Route
//----------------------------------------------------------------------------
Route::Route(float arrowLength) : m_RouteVAO(0), m_RoutePositionVBO(0), m_RouteColoursVBO(0),
    m_OverlayVAO(0), m_OverlayPositionVBO(0), m_OverlayColoursVBO(0)
{
    const GLfloat arrowPositions[] = {
        0.0f, 0.0f,
        0.0f, arrowLength,
    };

    const GLfloat arrowColours[] = {
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f
    };

    // Create a vertex array object to bind everything together
    glGenVertexArrays(1, &m_OverlayVAO);

    // Generate vertex buffer objects for positions and colours
    glGenBuffers(1, &m_OverlayPositionVBO);
    glGenBuffers(1, &m_OverlayColoursVBO);

    // Bind vertex array
    glBindVertexArray(m_OverlayVAO);

    // Bind and upload positions buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_OverlayPositionVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 2, arrowPositions, GL_STATIC_DRAW);

    // Set vertex pointer to stride over angles and enable client state in VAO
    glVertexPointer(2, GL_FLOAT, 0, BUFFER_OFFSET(0));
    glEnableClientState(GL_VERTEX_ARRAY);

     // Bind and upload colours buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_OverlayColoursVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 6, arrowColours, GL_STATIC_DRAW);

    // Set colour pointer and enable client state in VAO
    glColorPointer(3, GL_FLOAT, 0, BUFFER_OFFSET(0));
    glEnableClientState(GL_COLOR_ARRAY);
}
//----------------------------------------------------------------------------
Route::Route(float arrowLength, const std::string &filename, double waypointDistance) : Route(arrowLength)
{
    if(!load(filename, waypointDistance)) {
        throw std::runtime_error("Cannot load route");
    }
}
//----------------------------------------------------------------------------
Route::~Route()
{
    // Delete route objects
    glDeleteBuffers(1, &m_RoutePositionVBO);
    glDeleteVertexArrays(1, &m_RouteColoursVBO);
    glDeleteVertexArrays(1, &m_RouteVAO);

    // Delete overlay objects
    glDeleteBuffers(1, &m_OverlayPositionVBO);
    glDeleteBuffers(1, &m_OverlayColoursVBO);
    glDeleteVertexArrays(1, &m_OverlayVAO);
}
//----------------------------------------------------------------------------
bool Route::load(const std::string &filename, double waypointDistance)
{
    // Open file for binary IO
    std::ifstream input(filename, std::ios::binary);
    if(!input.good()) {
        std::cerr << "Cannot open route file:" << filename << std::endl;
        return false;
    }

    // Seek to end of file, get size and rewind
    input.seekg(0, std::ios_base::end);
    const std::streampos numPoints = input.tellg() / (sizeof(double) * 3);
    input.seekg(0);
    std::cout << "Route has " << numPoints << " points" << std::endl;

    {
        // Loop through components(X and Y, ignoring heading)
        std::vector<std::array<float, 2>> fullRoute(numPoints);
        for(unsigned int c = 0; c < 2; c++) {
            // Loop through points on path
            for(unsigned int i = 0; i < numPoints; i++) {
                // Read point component
                double pointPosition;
                input.read(reinterpret_cast<char*>(&pointPosition), sizeof(double));

                // Convert to float, scale to metres and insert into route
                fullRoute[i][c] = (float)pointPosition * (1.0f / 100.0f);
            }
        }

        // Reservve approximately correctly sized vector for waypoints
        m_Route.reserve(numPoints / 10);

        // Loop through points in full pat
        float lastX;
        float lastY;
        float distanceSinceLastPoint = 0.0f;
        for(unsigned int i = 0; i < numPoints; i++)
        {
            // If this isn't the first point
            const auto &p = fullRoute[i];
            if(i > 0) {
                // Calcualte distance to last point
                const float deltaX = p[0] - lastX;
                const float deltaY = p[1] - lastY;
                const float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));

                // Add to total
                distanceSinceLastPoint += distance;
            }

            // Update last point
            lastX = p[0];
            lastY = p[1];

            // If this is either the first point or we've gone over the waypoint distance
            if(i == 0 || distanceSinceLastPoint > waypointDistance) {
                // Add point to route
                m_Route.push_back(p);

                // Reset counter
                distanceSinceLastPoint = 0.0f;
            }
        }
    }

    // Create a vertex array object to bind everything together
    glGenVertexArrays(1, &m_RouteVAO);

    // Generate vertex buffer objects for positions and colours
    glGenBuffers(1, &m_RoutePositionVBO);
    glGenBuffers(1, &m_RouteColoursVBO);

    // Bind vertex array
    glBindVertexArray(m_RouteVAO);

    // Bind and upload positions buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_RoutePositionVBO);
    glBufferData(GL_ARRAY_BUFFER, m_Route.size() * sizeof(GLfloat) * 2, m_Route.data(), GL_STATIC_DRAW);

    // Set vertex pointer and enable client state in VAO
    glVertexPointer(2, GL_FLOAT, 0, BUFFER_OFFSET(0));
    glEnableClientState(GL_VERTEX_ARRAY);

    {
        // Bind and upload zeros to colour buffer
        std::vector<uint8_t> colours(m_Route.size() * 3, 0);
        glBindBuffer(GL_ARRAY_BUFFER, m_RouteColoursVBO);
        glBufferData(GL_ARRAY_BUFFER, m_Route.size() * sizeof(uint8_t) * 3, colours.data(), GL_STATIC_DRAW);

        // Set colour pointer and enable client state in VAO
        glColorPointer(3, GL_UNSIGNED_BYTE, 0, BUFFER_OFFSET(0));
        glEnableClientState(GL_COLOR_ARRAY);
    }
    return true;
}
//----------------------------------------------------------------------------
void Route::render(float antX, float antY, float antHeading) const
{
    // Bind route VAO
    glBindVertexArray(m_RouteVAO);

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.1f);
    glDrawArrays(GL_POINTS, 0, m_Route.size());

    glBindVertexArray(m_OverlayVAO);

    glTranslatef(antX, antY, 0.1f);
    glRotatef(-antHeading, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_LINES, 0, 2);
    glPopMatrix();
}
//----------------------------------------------------------------------------
bool Route::atDestination(float x, float y, float threshold) const
{
    // If route's empty, there is no destination so return false
    if(m_Route.empty()) {
        return false;
    }
    // Otherwise return true if
    else {
        return (distanceSquared(x, y, m_Route.back()[0], m_Route.back()[1]) < sqr(threshold));
    }
}
//----------------------------------------------------------------------------
std::tuple<float, size_t> Route::getDistanceToRoute(float x, float y) const
{
    // Loop through segments
    float minimumDistanceSquared = std::numeric_limits<float>::max();
    size_t nearestSegment;
    for(unsigned int s = 0; s < (m_Route.size() - 1); s++)
    {
        // Get positions of start and end of segment
        const float startX = m_Route[s][0];
        const float startY = m_Route[s][1];
        const float endX = m_Route[s + 1][0];
        const float endY = m_Route[s + 1][1];

        const float segmentLengthSquared = distanceSquared(startX, startY, endX, endY);

        // If segment has no length
        if(segmentLengthSquared == 0) {
            // Calculate distance from point to segment start (arbitrary)
            const float distanceToStartSquared = distanceSquared(startX, startY, x, y);

            // If this is closer than current minimum, update minimum and nearest segment
            if(distanceToStartSquared < minimumDistanceSquared) {
                minimumDistanceSquared = distanceToStartSquared;
                nearestSegment = s;
            }
        }
        else {
            // Calculate dot product of vector from start of segment and vector along segment and clamp
            float t = ((x - startX) * (endX - startY) + (y - startY) * (endY - startY)) / segmentLengthSquared;
            t = std::max(0.0f, std::min(1.0f, t));

            // Use this to project point onto segment
            const float projX = startX + (t * (endX - startX));
            const float projY = startY + (t * (endY - startY));

            // Calculate distance from this point to point
            const float distanceToSegmentSquared = distanceSquared(x, y, projX, projY);

            // If this is closer than current minimum, update minimum and nearest segment
            if(distanceToSegmentSquared < minimumDistanceSquared) {
                minimumDistanceSquared = distanceToSegmentSquared;
                nearestSegment = s;
            }
        }
    }

    // Return the minimum distance to the path and the segment in which this occured
    return std::make_tuple(sqrt(minimumDistanceSquared), nearestSegment);
}
//----------------------------------------------------------------------------
void Route::setWaypointFamiliarity(size_t pos, double familiarity)
{
    // Convert familiarity to a grayscale colour
    const uint8_t intensity = (uint8_t)std::min(255.0, std::max(0.0, std::round(255.0 * familiarity)));
    const uint8_t colour[3] = {intensity, intensity, intensity};

    // Update this positions colour in colour buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_RouteColoursVBO);
    glBufferSubData(GL_ARRAY_BUFFER, pos * sizeof(uint8_t) * 3, sizeof(uint8_t) * 3, colour);

}
//----------------------------------------------------------------------------
std::tuple<float, float, float> Route::operator[](size_t pos) const
{
    const float x = m_Route[pos][0];
    const float y = m_Route[pos][1];

    if(pos < (m_Route.size() - 1)) {
        const float nextX = m_Route[pos + 1][0];
        const float nextY = m_Route[pos + 1][1];

        const float deltaX = nextX - x;
        const float deltaY = nextY - y;

        return std::make_tuple(x, y, 90.0 + (radiansToDegrees * atan2(-deltaY, deltaX)));
    }
    else {
        return std::make_tuple(x, y, 0.0f);
    }
}