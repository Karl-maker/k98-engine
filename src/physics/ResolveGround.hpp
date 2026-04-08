void resolveGround(RigidBodyComponent& body, TransformComponent& transform, float groundHeight)
{
    float bottom = transform.position.y;

    if (bottom < groundHeight)
    {
        transform.position.y = groundHeight;

        if (body.velocity.y < 0)
            body.velocity.y = 0;

        body.isGrounded = true;
    }
    else
    {
        body.isGrounded = false;
    }
}