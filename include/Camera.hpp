#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vkpbr {
	class Camera {
	public:
		glm::vec3 rotation = glm::vec3();
		glm::vec3 position = glm::vec3();
		float rotationSpeed = 1.0f;
		float movementSpeed = 1.0f;

		bool updated = false;

		using Matrices = struct {
			glm::mat4 perspective;
			glm::mat4 view;
		};
		Matrices matrices;

		auto getNearPlane() -> float
		{
			return znear;
		}

		auto getFarPlane() -> float
		{
			return zfar;
		}

		auto setPerspective(float fov, float aspect, float znear, float zfar) -> void
		{
			this->fov = fov;
			this->znear = znear;
			this->zfar = zfar;
			matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		}

		auto updateAspectRatio(float aspect)
		{
			matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		}

		auto setPosition(glm::vec3 position) -> void
		{
			this->position = position;
			updateView();
		}

		auto setRotation(glm::vec3 rotation) ->void
		{
			this->rotation = rotation;
			updateView();
		}

		auto rotate(glm::vec3 delta) -> void
		{
			this->rotation += delta;
			updateView();
		}

		auto setTranslation(glm::vec3 translation) -> void
		{
			this->position = translation;
			updateView();
		}

		auto translate(glm::vec3 delta) -> void
		{
			this->position += delta;
			updateView();
		}

		auto update(float deltaTime) -> void
		{
			updated = false;

			auto front = glm::vec3{};
			front.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
			front.y = sin(glm::radians(rotation.x));
			front.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
			front = glm::normalize(front);

			const auto speed = movementSpeed * deltaTime;

			/* vyresit pohyb */

			updateView();
		}


	private:
		float fov;
		float znear;
		float zfar;

		auto updateView() -> void
		{
			auto rotation_matrix = glm::mat4(1.0f);
			auto transformation_matrix = glm::mat4{};

			rotation_matrix = glm::rotate(
				rotation_matrix,
				glm::radians(rotation.x),
				glm::vec3(1.0f, 0.0f, 0.0f)
			);

			rotation_matrix = glm::rotate(
				rotation_matrix,
				glm::radians(rotation.y),
				glm::vec3(0.0f, 1.0f, 0.0f)
			);

			rotation_matrix = glm::rotate(
				rotation_matrix,
				glm::radians(rotation.z),
				glm::vec3(0.0f, 0.0f, 1.0f)
			);

			transformation_matrix = glm::translate(glm::mat4(1.0f), position * glm::vec3(1.0f, 1.0f, -1.0f));

			matrices.view = rotation_matrix * transformation_matrix;

			updated = true;
		}
	};
}